#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include "common/Message.h"
#include "common/Protocol.h"
#include "network/FrameDecoder.h"

namespace {

using Clock = std::chrono::steady_clock;

struct ConnectionState {
    int fd = -1;
    int expected_task_id = 0;
    bool connected = false;
    bool failed = false;
    bool request_in_flight = false;
    int requests_sent = 0;
    int responses_received = 0;
    std::string output;
    std::size_t output_offset = 0;
    dts::FrameDecoder decoder;
    Clock::time_point request_start;
};

bool setNonBlocking(int fd) {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

bool parseEndpoint(const std::string& endpoint, std::string& ip, int& port) {
    const std::size_t pos = endpoint.rfind(':');
    if (pos == std::string::npos) return false;

    ip = endpoint.substr(0, pos);

    try {
        std::size_t parsed = 0;
        port = std::stoi(endpoint.substr(pos + 1), &parsed);
        if (parsed != endpoint.size() - pos - 1) return false;
    } catch (...) {
        return false;
    }

    return !ip.empty() && port > 0 && port <= 65535;
}

double percentile(std::vector<double> values, double p) {
    if (values.empty()) return 0.0;

    std::sort(values.begin(), values.end());
    const double position = p * static_cast<double>(values.size() - 1);
    const std::size_t lower = static_cast<std::size_t>(std::floor(position));
    const std::size_t upper = static_cast<std::size_t>(std::ceil(position));

    if (lower == upper) return values[lower];

    const double weight = position - static_cast<double>(lower);
    return values[lower] * (1.0 - weight) + values[upper] * weight;
}

void queueQuery(ConnectionState& state, int task_id) {
    dts::TaskQueryInfo query;
    query.task_id = task_id;

    dts::Message message;
    message.header.type = dts::MessageType::QUERY_TASK;
    message.data = dts::Protocol::serializeTaskQueryInfo(query);

    state.output = dts::Protocol::serialize(message);
    state.output_offset = 0;
    state.expected_task_id = task_id;
    state.request_in_flight = true;
    state.request_start = Clock::now();
    ++state.requests_sent;
}

bool flushOutput(ConnectionState& state) {
    while (state.output_offset < state.output.size()) {
        const char* data = state.output.data() + state.output_offset;
        const std::size_t remaining = state.output.size() - state.output_offset;

        const ssize_t n = ::send(state.fd, data, remaining, MSG_NOSIGNAL);
        if (n > 0) {
            state.output_offset += static_cast<std::size_t>(n);
            continue;
        }

        if (n < 0 && errno == EINTR) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return true;
        return false;
    }

    state.output.clear();
    state.output_offset = 0;
    return true;
}

bool modifyInterest(int epoll_fd, const ConnectionState& state) {
    epoll_event event{};
    event.data.fd = state.fd;
    event.events = EPOLLIN | EPOLLRDHUP;
    if (state.output_offset < state.output.size()) event.events |= EPOLLOUT;
    return ::epoll_ctl(epoll_fd, EPOLL_CTL_MOD, state.fd, &event) == 0;
}

void closeConnection(int epoll_fd, ConnectionState& state) {
    if (state.fd < 0) return;
    ::epoll_ctl(epoll_fd, EPOLL_CTL_DEL, state.fd, nullptr);
    ::close(state.fd);
    state.fd = -1;
}

} // namespace

int main(int argc, char* argv[]) {
    std::string endpoint;
    int connection_count = 0;
    int requests_per_connection = 0;
    int timeout_ms = 30000;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--master" && i + 1 < argc) {
            endpoint = argv[++i];
        } else if (arg == "--connections" && i + 1 < argc) {
            connection_count = std::atoi(argv[++i]);
        } else if (arg == "--requests-per-connection" && i + 1 < argc) {
            requests_per_connection = std::atoi(argv[++i]);
        } else if (arg == "--timeout-ms" && i + 1 < argc) {
            timeout_ms = std::atoi(argv[++i]);
        } else {
            std::cerr << "Usage: " << argv[0]
                      << " --master <ip:port>"
                      << " --connections <count>"
                      << " --requests-per-connection <count>"
                      << " [--timeout-ms <ms>]" << std::endl;
            return 1;
        }
    }

    if (endpoint.empty() || connection_count <= 0 || requests_per_connection <= 0 || timeout_ms <= 0) {
        std::cerr << "Invalid arguments" << std::endl;
        return 1;
    }

    std::string ip;
    int port = 0;
    if (!parseEndpoint(endpoint, ip, port)) {
        std::cerr << "Invalid master endpoint: " << endpoint << std::endl;
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(static_cast<uint16_t>(port));

    if (::inet_pton(AF_INET, ip.c_str(), &address.sin_addr) != 1) {
        std::cerr << "Invalid IPv4 address: " << ip << std::endl;
        return 1;
    }

    const int epoll_fd = ::epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd < 0) {
        std::cerr << "epoll_create1 failed: " << std::strerror(errno) << std::endl;
        return 1;
    }

    std::vector<ConnectionState> states(static_cast<std::size_t>(connection_count));
    std::vector<int> fd_to_index(1024, -1);
    std::vector<epoll_event> events(static_cast<std::size_t>(std::min(connection_count, 4096)));

    const std::uint64_t total_requests =
        static_cast<std::uint64_t>(connection_count) *
        static_cast<std::uint64_t>(requests_per_connection);

    int connected = 0;
    int connection_failed = 0;

    std::cout << "[Request Benchmark] target=" << endpoint << std::endl;
    std::cout << "[Request Benchmark] requested_connections=" << connection_count << std::endl;
    std::cout << "[Request Benchmark] requests_per_connection=" << requests_per_connection << std::endl;
    std::cout << "[Request Benchmark] total_requests=" << total_requests << std::endl;

    const auto connection_start = Clock::now();

    for (int i = 0; i < connection_count; ++i) {
        const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0 || !setNonBlocking(fd)) {
            if (fd >= 0) ::close(fd);
            states[static_cast<std::size_t>(i)].failed = true;
            ++connection_failed;
            continue;
        }

        auto& state = states[static_cast<std::size_t>(i)];
        state.fd = fd;

        if (static_cast<std::size_t>(fd) >= fd_to_index.size()) {
            fd_to_index.resize(static_cast<std::size_t>(fd) + 1, -1);
        }
        fd_to_index[static_cast<std::size_t>(fd)] = i;

        const int rc = ::connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address));
        if (rc < 0 && errno != EINPROGRESS) {
            state.failed = true;
            ++connection_failed;
            ::close(fd);
            state.fd = -1;
            continue;
        }

        epoll_event event{};
        event.data.fd = fd;
        event.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP;

        if (::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) != 0) {
            state.failed = true;
            ++connection_failed;
            ::close(fd);
            state.fd = -1;
            continue;
        }

        if (rc == 0) {
            state.connected = true;
            ++connected;
            modifyInterest(epoll_fd, state);
        }
    }

    const auto connection_deadline =
        connection_start + std::chrono::milliseconds(timeout_ms);

    while (connected + connection_failed < connection_count &&
           Clock::now() < connection_deadline) {
        const int ready = ::epoll_wait(
            epoll_fd,
            events.data(),
            static_cast<int>(events.size()),
            100
        );

        if (ready < 0) {
            if (errno == EINTR) continue;
            std::cerr << "epoll_wait failed during connection phase: "
                      << std::strerror(errno) << std::endl;
            break;
        }

        for (int i = 0; i < ready; ++i) {
            const int fd = events[static_cast<std::size_t>(i)].data.fd;
            const uint32_t event_mask = events[static_cast<std::size_t>(i)].events;

            if (fd < 0 || static_cast<std::size_t>(fd) >= fd_to_index.size()) continue;

            const int index = fd_to_index[static_cast<std::size_t>(fd)];
            if (index < 0) continue;

            auto& state = states[static_cast<std::size_t>(index)];
            if (state.fd != fd || state.failed || state.connected) continue;

            if (event_mask & (EPOLLOUT | EPOLLERR | EPOLLHUP)) {
                int socket_error = 0;
                socklen_t error_length = sizeof(socket_error);

                if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_length) != 0 ||
                    socket_error != 0) {
                    state.failed = true;
                    ++connection_failed;
                    closeConnection(epoll_fd, state);
                    continue;
                }

                state.connected = true;
                ++connected;

                if (!modifyInterest(epoll_fd, state)) {
                    state.connected = false;
                    state.failed = true;
                    --connected;
                    ++connection_failed;
                    closeConnection(epoll_fd, state);
                }
            }
            
        }
    }

    for (auto& state : states) {
        if (!state.connected && !state.failed) {
            state.failed = true;
            ++connection_failed;
            closeConnection(epoll_fd, state);
        }
    }

    const auto connection_end = Clock::now();
    const double connection_elapsed =
        std::chrono::duration<double>(connection_end - connection_start).count();

    const std::uint64_t unavailable_requests =
        static_cast<std::uint64_t>(connection_failed) *
        static_cast<std::uint64_t>(requests_per_connection);

    std::uint64_t requests_sent = 0;
    std::uint64_t responses = 0;
    std::uint64_t request_failed = 0;
    std::vector<double> latencies_ms;
    latencies_ms.reserve(
        static_cast<std::size_t>(connected) *
        static_cast<std::size_t>(requests_per_connection)
    );

    const auto request_start = Clock::now();

    for (int i = 0; i < connection_count; ++i) {
        auto& state = states[static_cast<std::size_t>(i)];
        if (!state.connected || state.failed) continue;

        queueQuery(state, 1000000000 + i);
        ++requests_sent;

        if (!flushOutput(state) || !modifyInterest(epoll_fd, state)) {
            state.failed = true;
            request_failed += static_cast<std::uint64_t>(
                requests_per_connection - state.responses_received
            );
            closeConnection(epoll_fd, state);
        }
    }

    const std::uint64_t expected_responses =
        static_cast<std::uint64_t>(connected) *
        static_cast<std::uint64_t>(requests_per_connection);

    const auto request_deadline =
        request_start + std::chrono::milliseconds(timeout_ms);

    while (Clock::now() < request_deadline &&
           responses + request_failed < expected_responses) {
        const int ready = ::epoll_wait(
            epoll_fd,
            events.data(),
            static_cast<int>(events.size()),
            100
        );

        if (ready < 0) {
            if (errno == EINTR) continue;
            std::cerr << "epoll_wait failed during request phase: "
                      << std::strerror(errno) << std::endl;
            break;
        }

        for (int i = 0; i < ready; ++i) {
            const int fd = events[static_cast<std::size_t>(i)].data.fd;
            const uint32_t event_mask = events[static_cast<std::size_t>(i)].events;

            if (fd < 0 || static_cast<std::size_t>(fd) >= fd_to_index.size()) continue;

            const int index = fd_to_index[static_cast<std::size_t>(fd)];
            if (index < 0) continue;

            auto& state = states[static_cast<std::size_t>(index)];
            if (state.fd != fd || state.failed || !state.connected) continue;

            if ((event_mask & EPOLLOUT) && state.output_offset < state.output.size()) {
                if (!flushOutput(state) || !modifyInterest(epoll_fd, state)) {
                    state.failed = true;
                    request_failed += static_cast<std::uint64_t>(
                        requests_per_connection - state.responses_received
                    );
                    closeConnection(epoll_fd, state);
                    continue;
                }
            }

            if (event_mask & EPOLLIN) {
                char buffer[4096];

                while (true) {
                    const ssize_t n = ::recv(fd, buffer, sizeof(buffer), 0);

                    if (n > 0) {
                        auto messages = state.decoder.feed(
                            std::string(buffer, static_cast<std::size_t>(n))
                        );

                        if (state.decoder.hasError()) {
                            state.failed = true;
                            request_failed += static_cast<std::uint64_t>(
                                requests_per_connection - state.responses_received
                            );
                            closeConnection(epoll_fd, state);
                            break;
                        }

                        for (const auto& message : messages) {
                            if (message.header.type != dts::MessageType::TASK_STATUS ||
                                !state.request_in_flight) {
                                state.failed = true;
                                request_failed += static_cast<std::uint64_t>(
                                    requests_per_connection - state.responses_received
                                );
                                closeConnection(epoll_fd, state);
                                break;
                            }

                            dts::TaskStatusInfo status;

                            try {
                                status = dts::Protocol::deserializeTaskStatusInfo(message.data);
                            } catch (...) {
                                state.failed = true;
                                request_failed += static_cast<std::uint64_t>(
                                    requests_per_connection - state.responses_received
                                );
                                closeConnection(epoll_fd, state);
                                break;
                            }

                            if (status.task_id != state.expected_task_id || status.found) {
                                state.failed = true;
                                request_failed += static_cast<std::uint64_t>(
                                    requests_per_connection - state.responses_received
                                );
                                closeConnection(epoll_fd, state);
                                break;
                            }
                            
                            const auto now = Clock::now();
                            latencies_ms.push_back(
                                std::chrono::duration<double, std::milli>(
                                    now - state.request_start
                                ).count()
                            );

                            state.request_in_flight = false;
                            ++state.responses_received;
                            ++responses;

                            if (state.responses_received < requests_per_connection) {
                                queueQuery(state, 1000000000 + index);
                                ++requests_sent;

                                if (!flushOutput(state) || !modifyInterest(epoll_fd, state)) {
                                    state.failed = true;
                                    request_failed += static_cast<std::uint64_t>(
                                        requests_per_connection - state.responses_received
                                    );
                                    closeConnection(epoll_fd, state);
                                    break;
                                }
                            } else if (!modifyInterest(epoll_fd, state)) {
                                state.failed = true;
                                closeConnection(epoll_fd, state);
                                break;
                            }
                        }

                        if (state.failed) break;
                        continue;
                    }

                    if (n == 0) {
                        state.failed = true;
                        request_failed += static_cast<std::uint64_t>(
                            requests_per_connection - state.responses_received
                        );
                        closeConnection(epoll_fd, state);
                        break;
                    }

                    if (errno == EINTR) continue;
                    if (errno == EAGAIN || errno == EWOULDBLOCK) break;

                    state.failed = true;
                    request_failed += static_cast<std::uint64_t>(
                        requests_per_connection - state.responses_received
                    );
                    closeConnection(epoll_fd, state);
                    break;
                }
            }

            if (!state.failed && (event_mask & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))) {
                state.failed = true;
                request_failed += static_cast<std::uint64_t>(
                    requests_per_connection - state.responses_received
                );
                closeConnection(epoll_fd, state);
            }
        }
    }

    const auto request_end = Clock::now();
    const double request_elapsed =
        std::chrono::duration<double>(request_end - request_start).count();

    const std::uint64_t timed_out =
        expected_responses > responses + request_failed
            ? expected_responses - responses - request_failed
            : 0;

    const std::uint64_t sent_without_response =
        requests_sent > responses ? requests_sent - responses : 0;

    for (auto& state : states) closeConnection(epoll_fd, state);
    ::close(epoll_fd);

    std::cout << "[Request Benchmark] connected=" << connected << std::endl;
    std::cout << "[Request Benchmark] connection_failed=" << connection_failed << std::endl;
    std::cout << "[Request Benchmark] unavailable_requests=" << unavailable_requests << std::endl;
    std::cout << "[Request Benchmark] requests_sent=" << requests_sent << std::endl;
    std::cout << "[Request Benchmark] responses=" << responses << std::endl;
    std::cout << "[Request Benchmark] request_failed=" << request_failed << std::endl;
    std::cout << "[Request Benchmark] timed_out=" << timed_out << std::endl;
    std::cout << "[Request Benchmark] sent_without_response=" << sent_without_response << std::endl;
    std::cout << "[Request Benchmark] connection_elapsed=" << connection_elapsed << " s" << std::endl;
    std::cout << "[Request Benchmark] request_elapsed=" << request_elapsed << " s" << std::endl;

    if (request_elapsed > 0.0) {
        std::cout << "[Request Benchmark] throughput="
                  << static_cast<double>(responses) / request_elapsed
                  << " requests/s" << std::endl;
    }

    if (!latencies_ms.empty()) {
        std::cout << "[Request Benchmark] latency_p50="
                  << percentile(latencies_ms, 0.50) << " ms" << std::endl;
        std::cout << "[Request Benchmark] latency_p95="
                  << percentile(latencies_ms, 0.95) << " ms" << std::endl;
        std::cout << "[Request Benchmark] latency_p99="
                  << percentile(latencies_ms, 0.99) << " ms" << std::endl;
    }

    return connected == connection_count &&
           responses == total_requests &&
           request_failed == 0 &&
           timed_out == 0
               ? 0
               : 1;
}