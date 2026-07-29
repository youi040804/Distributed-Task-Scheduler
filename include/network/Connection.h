/*
 * Connection.h
 * 网络连接类，封装 TCP 连接的发送/接收/关闭
 */
#pragma once

#include <netinet/in.h>
#include<string>
#include "../common/Message.h"

namespace dts{
class Connection{
private:
    int fd_;
    sockaddr_in peer_addr_;
public:

    explicit Connection(int fd ,sockaddr_in addr);
    //bool send(const std::string &data);
    //将原来发送临时字符串修改为真正的Message格式
    bool sendMessage(Message&msg);
    //底层send()
    bool send(const std::string&data,uint32_t total_len);

    Message receiveMessage();
    //底层recv
    std::string recv();

    void disconnect();
    int fd()const;

};
}