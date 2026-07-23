/*
 * tests/network/network_test_client.cpp
 * TCP 客户端测试
 */

#include "../../include/network/TCPClient.h"
#include <iostream>

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
            bool sent = conn->send("hello network");
            std::cout << "Sent: " << sent << std::endl;
        }
    }

    std::cout << "Client done." << std::endl;
    return 0;
}