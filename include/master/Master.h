/*
 * Master.h
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

#include"../network/TCPServer.h"
#include"../common/WorkerInfo.h"
#include "WorkerManager.h"

namespace dts{
class Master{
private:
int port_;
std::shared_ptr<TCPServer>master_server_;

WorkerManager worker_manager_;
public:
Master(int master_port);
bool start(TCPServer& server);
void handleWorkerRegister(std::string&register_data);
void run();
};



 }