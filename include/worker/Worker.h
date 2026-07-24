/*
 * Worekr.h
 */
#pragma once
#include<sys/socket.h>
#include<memory>
#include<string>
#include"../network/TCPClient.h"
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

bool registerToMaster(std::string&data);
bool start(int worker_id,std::string&data,
    const std::string&master_ip,int master_port);


};
}