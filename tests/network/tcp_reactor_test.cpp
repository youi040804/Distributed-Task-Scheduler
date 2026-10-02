#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "common/Message.h"
#include "common/Protocol.h"
#include "network/Connection.h"
#include "network/TCPReactor.h"
#include "network/TCPServer.h"

namespace {

constexpr int PORT = 19092;

int connectClient() {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    assert(::connect(fd, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == 0);
    return fd;
}

std::string makeFrame(dts::MessageType type, const std::string& data) {
    dts::Message message;
    message.header.type = type;
    message.data = data;
    return dts::Protocol::serialize(message);
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

    const int client1 = connectClient();
    const int client2 = connectClient();

    const auto accept_messages = reactor.pollOnce(1000);
    assert(accept_messages.empty());

    const std::string frame1 = makeFrame(dts::MessageType::SUBMIT_TASK, "client-one");
    const std::string frame2 = makeFrame(dts::MessageType::QUERY_TASK, "client-two");

    sendAll(client1, frame1);
    sendAll(client2, frame2);

    std::vector<std::pair<std::shared_ptr<dts::Connection>, dts::Message>> messages;

    for (int i = 0; i < 3 && messages.size() < 2; ++i) {
        const auto batch = reactor.pollOnce(1000);
        messages.insert(messages.end(), batch.begin(), batch.end());
    }

    assert(messages.size() == 2);

    bool found_client_one = false;
    bool found_client_two = false;
    std::shared_ptr<dts::Connection> server_connection1;

    for (const auto& item : messages) {
        const auto& connection = item.first;
        const auto& message = item.second;

        if (message.header.type == dts::MessageType::SUBMIT_TASK && message.data == "client-one") {
            found_client_one = true;
            server_connection1 = connection;
        }

        if (message.header.type == dts::MessageType::QUERY_TASK && message.data == "client-two") {
            found_client_two = true;
        }
    }

    assert(found_client_one);
    assert(found_client_two);
    assert(server_connection1 != nullptr);

    const std::string partial_frame = makeFrame(dts::MessageType::SUBMIT_TASK, "partial-message");

    const std::size_t split = partial_frame.size() / 2;
    const std::string first_half = partial_frame.substr(0, split);
    const std::string second_half = partial_frame.substr(split);

    sendAll(client1, first_half);

    const auto first_half_messages = reactor.pollOnce(1000);
    assert(first_half_messages.empty());

    sendAll(client1, second_half);

    const auto second_half_messages = reactor.pollOnce(1000);
    assert(second_half_messages.size() == 1);
    assert(second_half_messages[0].second.header.type == dts::MessageType::SUBMIT_TASK);
    assert(second_half_messages[0].second.data == "partial-message");

    dts::Message response;
    response.header.type = dts::MessageType::TASK_STATUS;
    response.data = "reactor-output-test";

    assert(reactor.sendMessage(server_connection1, response));
    assert(server_connection1->hasPendingOutput());

    for (int i = 0; i < 3 && server_connection1->hasPendingOutput(); ++i) {
        const auto output_messages = reactor.pollOnce(1000);
        assert(output_messages.empty());
    }

    assert(!server_connection1->hasPendingOutput());

    sockaddr_in dummy_addr{};
    dts::Connection client1_connection(client1, dummy_addr);

    const dts::Message received_response = client1_connection.receiveMessage();

    assert(received_response.header.type == dts::MessageType::TASK_STATUS);
    assert(received_response.data == "reactor-output-test");

    client1_connection.disconnect();
    ::close(client2);

    reactor.pollOnce(1000);
    server.stop();

    std::cout << "tcp_reactor_test passed" << std::endl;
    return 0;
}