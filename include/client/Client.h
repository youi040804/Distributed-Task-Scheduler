/*
*Client.h
*/
#pragma once
#include<memory>
#include"network/TCPClient.h"
#include "common/Message.h"  
#include"common/Protocol.h"

namespace dts{
class Client{
private:
    int client_id_;
    sockaddr_in master_addr_ ;
    std::unique_ptr<TCPClient>client_;

public:
    explicit Client(int client_id);
    void setMasterAddress(const std::string& master_ip, int master_port);
    bool connectMaster();

    bool sendToMaster(const Message& msg); 
  
    bool submitTask(const TaskSubmitInfo& info);

    void stop();

};
}
