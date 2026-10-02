#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common/Message.h"
#include "common/Protocol.h"
#include "network/Connection.h"
#include "network/TCPReactor.h"
#include "network/TCPServer.h"

namespace {

constexpr int PORT = 19093;

int connectClient() {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    const int result = ::connect(fd, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
    assert(result == 0);

    return fd;
}

void sendAll(int fd, const std::string& data) {
    std::size_t sent = 0;

    while (sent < data.size()) {
        const ssize_t n = ::send(fd, data.data() + sent, data.size() - sent, 0);
        assert(n > 0);
        sent += static_cast<std::size_t>(n);
    }
}

} // namespace

int main() {
    dts::TCPServer server(PORT);
    assert(server.start());

    dts::TCPReactor reactor(server);
    assert(reactor.start());

    const int client_fd = connectClient();

    const auto accept_messages = reactor.pollOnce(1000);
    assert(accept_messages.empty());

    dts::Message request;
    request.header.type = dts::MessageType::QUERY_TASK;
    request.data = "half-close-request";

    const std::string raw_request = dts::Protocol::serialize(request);
    sendAll(client_fd, raw_request);

    // 关闭写方向，但仍保留读方向等待服务端响应
    assert(::shutdown(client_fd, SHUT_WR) == 0);

    std::vector<std::pair<std::shared_ptr<dts::Connection>, dts::Message>> messages;

    for (int i = 0; i < 3 && messages.empty(); ++i) {
        const auto batch = reactor.pollOnce(1000);
        messages.insert(messages.end(), batch.begin(), batch.end());
    }

    assert(messages.size() == 1);

    const auto server_connection = messages[0].first;
    const auto& received_request = messages[0].second;

    assert(server_connection != nullptr);
    assert(received_request.header.type == dts::MessageType::QUERY_TASK);
    assert(received_request.data == "half-close-request");
    assert(server_connection->isPeerReadClosed());
    assert(!server_connection->hasFatalReceiveError());

    dts::Message response;
    response.header.type = dts::MessageType::TASK_STATUS;
    response.data = "half-close-response";

    assert(reactor.sendMessage(server_connection, response));

    for (int i = 0; i < 5 && server_connection->hasPendingOutput(); ++i) {
        reactor.pollOnce(1000);
    }

    assert(!server_connection->hasPendingOutput());

    sockaddr_in dummy_addr{};
    dts::Connection client_connection(client_fd, dummy_addr);

    const dts::Message received_response = client_connection.receiveMessage();

    assert(received_response.header.type == dts::MessageType::TASK_STATUS);
    assert(received_response.data == "half-close-response");

    // peer EOF + output flush 完成后，服务端连接应被回收
    assert(server_connection->fd() == -1);

    client_connection.disconnect();
    server.stop();

    std::cout << "reactor_half_close_test passed" << std::endl;
    return 0;
}