/*
 * Connection.h
 * 网络连接类，封装 TCP 连接的发送/接收/关闭
 */
#pragma once

#include <netinet/in.h>
#include<string>
#include "common/Message.h"

namespace dts{
class Connection{
private:
    int fd_;
    sockaddr_in peer_addr_;
public:

    explicit Connection(int fd ,sockaddr_in addr);

    bool sendMessage(Message&msg);
    bool send(const std::string&data,uint32_t total_len);//底层send()

    Message receiveMessage();
    std::string recv();//底层recv

    void disconnect();
    int fd()const;

};
}