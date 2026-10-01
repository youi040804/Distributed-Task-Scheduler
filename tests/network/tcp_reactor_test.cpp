#include <algorithm>    // std::find_if
#include <cassert>     
#include <iostream>    
#include <string>       
#include <sys/socket.h> // socket, connect, send
#include <unistd.h>     // close
#include <arpa/inet.h>  // sockaddr_in, htons, htonl

#include "common/Message.h"
#include "common/Protocol.h"
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

std::string makeFrame( dts::MessageType type, const std::string& data ){
    dts::Message message;
    message.header.type = type;
    message.data = data;

    return dts::Protocol::serialize(message);
}

void sendAll(int fd, const std::string& data) {
    std::size_t sent = 0;

    while (sent < data.size()) {
        const ssize_t n = ::send( fd, data.data() + sent, data.size() - sent, 0 );

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

    // --------------------------------------------------
    // 1. 建立两个客户端连接
    // --------------------------------------------------

    const int client1 = connectClient();
    const int client2 = connectClient();

    // listen_fd 应该已经 readable
    // 第一轮 poll 负责 accept，并把新 Connection 注册进 epoll
    const auto accept_messages = reactor.pollOnce(1000);

    // 这里只发生连接建立，没有业务 Message
    assert(accept_messages.empty());

    // --------------------------------------------------
    // 2. 两个客户端分别发送完整 Message
    // --------------------------------------------------

    const std::string frame1 = makeFrame( dts::MessageType::SUBMIT_TASK, "client-one" );
    const std::string frame2 = makeFrame( dts::MessageType::QUERY_TASK, "client-two" );

    sendAll(client1, frame1);
    sendAll(client2, frame2);

    const auto messages = reactor.pollOnce(1000);

    assert(messages.size() == 2);

    bool found_client_one = false;
    bool found_client_two = false;

    for (const auto& item : messages) {
        const auto& message = item.second;

        if (message.header.type == dts::MessageType::SUBMIT_TASK 
                && message.data == "client-one") {
            found_client_one = true;
        }

        if (message.header.type == dts::MessageType::QUERY_TASK 
                && message.data == "client-two") {
            found_client_two = true;
        }
    }

    assert(found_client_one);
    assert(found_client_two);

    // --------------------------------------------------
    // 3. 验证跨两轮 pollOnce() 的半帧
    // --------------------------------------------------

    const std::string partial_frame = makeFrame( dts::MessageType::SUBMIT_TASK, "partial-message" );

    const std::size_t split = partial_frame.size() / 2;
    const std::string first_half = partial_frame.substr(0, split);
    const std::string second_half = partial_frame.substr(split);

    sendAll(client1, first_half);

    // fd 会 readable，但 FrameDecoder 还不能组成完整 Message
    const auto first_half_messages = reactor.pollOnce(1000);

    assert(first_half_messages.empty());

    // Connection 没有销毁，FrameDecoder 中仍保留前半帧
    sendAll(client1, second_half);

    const auto second_half_messages = reactor.pollOnce(1000);

    assert(second_half_messages.size() == 1);
    assert( second_half_messages[0].second.header.type == dts::MessageType::SUBMIT_TASK );
    assert( second_half_messages[0].second.data == "partial-message" );

    ::close(client1);
    ::close(client2);

    // 让 Reactor 观察 peer close，并清理服务端 Connection
    reactor.pollOnce(1000);

    server.stop();

    std::cout << "tcp_reactor_test passed" << std::endl;

    return 0;
}