/*
 * tests/network/network_test_server.cpp
 * TCP 服务器测试
 */

#include "network/TCPServer.h"
#include <iostream>
#include <unistd.h>

int main() {
    dts::TCPServer server(8888);
    server.start();

    std::cout << "waiting connection..." << std::endl;

    auto conn = server.acceptConnection();
    if (conn) {
        std::string msg = conn->recv();
        std::cout << "received:" << std::endl;
        std::cout << msg << std::endl;
    }

    server.stop();
    return 0;
}