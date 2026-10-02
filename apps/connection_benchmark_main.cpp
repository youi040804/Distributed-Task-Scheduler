#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
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

namespace {

struct ConnectionState {
    int fd = -1;
    bool connected = false;
    bool failed = false;
};

void printUsage(const char* program) {
    std::cout
        << "Usage:\n"
        << "  " << program
        << " --master <ip:port>"
        << " --connections <count>"
        << " [--timeout-ms <milliseconds>]\n";
}

bool parseInt(const char* text, int& value) {
    try {
        std::size_t pos = 0;
        const int parsed = std::stoi(text, &pos);
        if (pos != std::strlen(text)) return false;
        value = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

bool parseEndpoint(const std::string& endpoint, std::string& ip, int& port) {
    const auto pos = endpoint.rfind(':');
    if (pos == std::string::npos || pos == 0 || pos + 1 >= endpoint.size()) return false;

    ip = endpoint.substr(0, pos);
    if (!parseInt(endpoint.substr(pos + 1).c_str(), port)) return false;

    return port > 0 && port <= 65535;
}

bool setNonBlocking(int fd) {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

void closeConnection(ConnectionState& connection) {
    if (connection.fd >= 0) {
        ::close(connection.fd);
        connection.fd = -1;
    }
}

} // namespace

int main(int argc, char* argv[]) {
    std::string master_ip;
    int master_port = 0;
    int connection_count = 0;
    int timeout_ms = 10000;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--master" && i + 1 < argc) {
            if (!parseEndpoint(argv[++i], master_ip, master_port)) {
                std::cerr << "Invalid master endpoint\n";
                return EXIT_FAILURE;
            }
        } else if (arg == "--connections" && i + 1 < argc) {
            if (!parseInt(argv[++i], connection_count) || connection_count <= 0) {
                std::cerr << "Invalid connection count\n";
                return EXIT_FAILURE;
            }
        } else if (arg == "--timeout-ms" && i + 1 < argc) {
            if (!parseInt(argv[++i], timeout_ms) || timeout_ms <= 0) {
                std::cerr << "Invalid timeout\n";
                return EXIT_FAILURE;
            }
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return EXIT_SUCCESS;
        } else {
            printUsage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (master_ip.empty() || master_port == 0 || connection_count <= 0) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    sockaddr_in master_addr{};
    master_addr.sin_family = AF_INET;
    master_addr.sin_port = htons(master_port);

    if (::inet_pton(AF_INET, master_ip.c_str(), &master_addr.sin_addr) != 1) {
        std::cerr << "Invalid Master IP\n";
        return EXIT_FAILURE;
    }

    const int epoll_fd = ::epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd < 0) {
        std::perror("epoll_create1");
        return EXIT_FAILURE;
    }

    std::vector<ConnectionState> connections(connection_count);
    std::vector<epoll_event> events(std::min(connection_count, 4096));

    int immediate_connected = 0;
    int pending = 0;
    int failed = 0;

    std::cout
        << "[Connection Benchmark] target=" << master_ip << ":" << master_port << "\n"
        << "[Connection Benchmark] requested_connections=" << connection_count << "\n"
        << "[Connection Benchmark] timeout_ms=" << timeout_ms
        << std::endl;

    const auto start_time = std::chrono::steady_clock::now();

    for (int i = 0; i < connection_count; ++i) {
        const int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);

        if (fd < 0) {
            connections[i].failed = true;
            ++failed;
            continue;
        }

        connections[i].fd = fd;

        if (!setNonBlocking(fd)) {
            closeConnection(connections[i]);
            connections[i].failed = true;
            ++failed;
            continue;
        }

        const int result = ::connect(
            fd,
            reinterpret_cast<sockaddr*>(&master_addr),
            sizeof(master_addr)
        );

        if (result == 0) {
            connections[i].connected = true;
            ++immediate_connected;
            continue;
        }

        if (errno != EINPROGRESS) {
            closeConnection(connections[i]);
            connections[i].failed = true;
            ++failed;
            continue;
        }

        epoll_event event{};
        event.events = EPOLLOUT | EPOLLERR | EPOLLHUP;
        event.data.u32 = static_cast<std::uint32_t>(i);

        if (::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) < 0) {
            closeConnection(connections[i]);
            connections[i].failed = true;
            ++failed;
            continue;
        }

        ++pending;
    }

    const auto deadline = start_time + std::chrono::milliseconds(timeout_ms);

    while (pending > 0) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) break;

        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - now
        ).count();

        const int wait_ms = static_cast<int>(std::min<std::int64_t>(remaining, 1000));

        const int ready = ::epoll_wait(
            epoll_fd,
            events.data(),
            static_cast<int>(events.size()),
            wait_ms
        );

        if (ready < 0) {
            if (errno == EINTR) continue;
            std::perror("epoll_wait");
            break;
        }

        for (int i = 0; i < ready; ++i) {
            const std::size_t index = events[i].data.u32;
            if (index >= connections.size()) continue;

            auto& connection = connections[index];
            if (connection.connected || connection.failed || connection.fd < 0) continue;

            int socket_error = 0;
            socklen_t error_length = sizeof(socket_error);

            if (::getsockopt(
                    connection.fd,
                    SOL_SOCKET,
                    SO_ERROR,
                    &socket_error,
                    &error_length
                ) < 0 || socket_error != 0) {

                ::epoll_ctl(epoll_fd, EPOLL_CTL_DEL, connection.fd, nullptr);
                closeConnection(connection);
                connection.failed = true;
                ++failed;
                --pending;
                continue;
            }

            ::epoll_ctl(epoll_fd, EPOLL_CTL_DEL, connection.fd, nullptr);
            connection.connected = true;
            --pending;
        }
    }

    int timed_out = 0;

    for (auto& connection : connections) {
        if (!connection.connected && !connection.failed) {
            ++timed_out;
            if (connection.fd >= 0) {
                ::epoll_ctl(epoll_fd, EPOLL_CTL_DEL, connection.fd, nullptr);
                closeConnection(connection);
            }
        }
    }

    const auto end_time = std::chrono::steady_clock::now();
    const std::chrono::duration<double> elapsed = end_time - start_time;

    int connected = 0;

    for (const auto& connection : connections) {
        if (connection.connected) ++connected;
    }

    const double success_rate =
        connection_count > 0
            ? static_cast<double>(connected) * 100.0 / connection_count
            : 0.0;

    const double connection_rate =
        elapsed.count() > 0.0
            ? static_cast<double>(connected) / elapsed.count()
            : 0.0;

    std::cout
        << "[Connection Benchmark] connected=" << connected << "\n"
        << "[Connection Benchmark] failed=" << failed << "\n"
        << "[Connection Benchmark] timed_out=" << timed_out << "\n"
        << "[Connection Benchmark] success_rate=" << success_rate << "%\n"
        << "[Connection Benchmark] elapsed=" << elapsed.count() << " s\n"
        << "[Connection Benchmark] connection_rate=" << connection_rate << " connections/s"
        << std::endl;

    std::cout << "[Connection Benchmark] holding connections for 3 seconds..." << std::endl;
    ::sleep(3);

    for (auto& connection : connections) closeConnection(connection);

    ::close(epoll_fd);

    return connected == connection_count ? EXIT_SUCCESS : EXIT_FAILURE;
}