/*
 * tests/network/network_test_client.cpp
 * TCP 客户端测试
 */

#include "network/TCPClient.h"
#include <iostream>
#include <string>

int main() {
    std::cout << "Client starting..." << std::endl;

    dts::TCPClient client("127.0.0.1", 8888);

    std::cout << "Connecting..." << std::endl;
    bool connected = client.connect();
    std::cout << "Connected: " << connected << std::endl;

    if (connected) {
        auto conn = client.getConnection();
        if (conn) {
            std::cout << "Sending..." << std::endl;
            std::string msg = "hello network";
            bool sent = conn->send(msg, msg.size());
            std::cout << "Sent: " << sent << std::endl;
        }
    }

    std::cout << "Client done." << std::endl;
    return 0;
}