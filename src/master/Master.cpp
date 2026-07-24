/*
 * Master.cpp
 * Master类的实现
 */

#pragma once

// #include "Connection.h"
// #include<unordered_map>
// #include<memory>
// class TCPServer{
// private:
// int port_;
// int listen_fd_;
// std::unordered_map<int,std::shared_ptr<Connection>> connections_;
// public:
//     explicit TCPServer(int port);
//     bool start();
//     std::shared_ptr<Connection> acceptConnection();
//     void stop();

// #include"../network/TCPServer.h"
// #include "WorkerManager.h"

// namespace dts{
// class Master{
// private:
// int port_;
// TCPServer master_server_;
//std::shared_ptr<TCPServer>master_server_;
//
// WorkerManager worker_manager_;
// public:
// Master(int master_port);
// bool start(TCPServer& server);
// void handleWorkerRegister(std::string&register_data);
// void run();
// };

#include"../../include/master/Master.h"
namespace dts{

Master::Master(int master_port):port_(master_port),master_server_(nullptr){
}

bool Master::start(TCPServer&server){
master_server_=std::make_shared<TCPServer>(port_);

int master_start_result=master_server_->start();
if(!master_start_result){
    perror("master start failed");
    return false;
}
return true;
}
void Master::handleWorkerRegister(std::string&register_data){
//1.解析消息
// 这里我们假设消息格式: "REGISTER_WORKER 192.168.1.100:8080 8"
//先手动进行截取，后续再替换为真实的消息类型。

//我不太懂这里应该如何处理，我猜想的是，接到
    // std::shared_ptr<Connection> TCPServer::acceptConnection(){
    //     sockaddr_in client_addr={};
    //     socklen_t addrlen=sizeof(client_addr);
    //     int client_fd=accept(listen_fd_,(sockaddr*)&client_addr,&addrlen);
    //     if(client_fd<0){
    //         perror("Server acceptConnection failed!");
    //         return nullptr;
    //     }
    //     connections_[client_fd] = std::make_shared<dts::Connection>(client_fd, client_addr);
    //     return connections_[client_fd];
    // }


}
void Master::run(){

}


}