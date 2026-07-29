/*
 * Worker.h
 * Worker 节点类，负责连接 Master、注册自身、执行任务
 */
#pragma once
#include<sys/socket.h>
#include<memory>
#include<string>
#include"../network/TCPClient.h"
#include "../common/Message.h"  
namespace dts{
class Worker{
private:
int worker_id_;
sockaddr_in   master_addr_ ;
std::unique_ptr<TCPClient>worker_client_;

public:
explicit Worker(int worker_id);
void setMasterAddress(const std::string& master_ip, int master_port);
bool connectMaster();

bool registerToMaster(Message&msg);
bool start(Message&msg,const std::string&master_ip,int master_port);


};
}