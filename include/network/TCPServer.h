/*
 * TCPServer.h
 * TCP 服务器类，监听端口、接受连接、管理客户端
 */
#pragma once

#include "Connection.h"
#include<unordered_map>
#include<memory>
namespace dts{
class TCPServer{
private:
int port_;
int listen_fd_;
std::unordered_map<int,std::shared_ptr<Connection>> connections_;
public:
    explicit TCPServer(int port);
    bool start();
    std::shared_ptr<Connection> acceptConnection();
    void stop();

};
}