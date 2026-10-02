#include <arpa/inet.h>
#include <cassert>
#include <chrono>
#include <iostream>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#include "network/TCPReactor.h"
#include "network/TCPServer.h"

int main() {
    constexpr int port = 19094;

    dts::TCPServer server(port);
    assert(server.start());

    dts::TCPReactor reactor(server);
    assert(reactor.start());

    const int client_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    assert(client_fd >= 0);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    assert(::inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) == 1);
    assert(::connect(client_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);

    std::shared_ptr<dts::Connection> server_connection;

    for (int i = 0; i < 20 && !server_connection; ++i) {
        reactor.pollOnce(50);

        for (int fd = 0; fd < 1024; ++fd) {
            auto connection = server.getConnection(fd);
            if (connection) {
                server_connection = connection;
                break;
            }
        }
    }

    assert(server_connection);
    const int server_fd = server_connection->fd();
    assert(server_fd >= 0);

    ::close(client_fd);

    for (int i = 0; i < 20 && server.getConnection(server_fd); ++i) {
        reactor.pollOnce(50);
    }

    assert(server.getConnection(server_fd) == nullptr);
    assert(server_connection->fd() == -1);

    const auto start = std::chrono::steady_clock::now();
    reactor.pollOnce(100);
    const auto end = std::chrono::steady_clock::now();

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    assert(elapsed >= 50);

    server.stop();

    std::cout << "reactor_peer_close_test passed" << std::endl;
    return 0;
}