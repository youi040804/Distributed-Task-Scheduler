#include <cassert>      
#include <fcntl.h>      // fcntl, F_GETFL, O_NONBLOCK
#include <iostream>     
#include <memory>      
#include <sys/socket.h> // socket
#include <unistd.h>     // close
#include <arpa/inet.h>  // sockaddr_in, htons, htonl

#include "network/TCPServer.h"

int connectClient(int port) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    assert(fd >= 0);

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    assert( ::connect( fd, reinterpret_cast<sockaddr*>(&server_addr), 
                        sizeof(server_addr) ) == 0 );

    return fd;
}

void testAcceptAvailable() {
    constexpr int PORT = 19091;

    dts::TCPServer server(PORT);

    assert(server.start());
    assert(server.listenFd() >= 0);

    // Reactor 模式下 listening socket 必须是 non-blocking
    assert(server.setListenNonBlocking());

    const int listen_flags = ::fcntl(server.listenFd(), F_GETFL, 0);

    assert(listen_flags != -1);
    assert((listen_flags & O_NONBLOCK) != 0);

    // 在 accept 前先建立多个待处理连接
    const int client1 = connectClient(PORT);
    const int client2 = connectClient(PORT);
    const int client3 = connectClient(PORT);

    const auto connections = server.acceptAvailable();

    assert(connections.size() == 3);

    // acceptAvailable 创建出来的 Connection也必须处于 non-blocking 模式
        for (const auto& connection : connections) {
        assert(connection);

        const int flags = ::fcntl(connection->fd(), F_GETFL, 0);

        assert(flags != -1);
        assert((flags & O_NONBLOCK) != 0);
    }

    // accept queue 已经被上一轮 drain 到 EAGAIN
    const auto empty = server.acceptAvailable();
    assert(empty.empty());

    ::close(client1);
    ::close(client2);
    ::close(client3);

    server.stop();
}

int main() {
    testAcceptAvailable();

    std::cout << "nonblocking_accept_test passed" << std::endl;

    return 0;
}