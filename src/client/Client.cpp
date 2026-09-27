/*
*Client.cpp
*/
#include <cstring>
#include <arpa/inet.h>
#include<iostream>
#include"client/Client.h"
namespace dts{

    Client::Client(int client_id)
        :client_id_(client_id),
         client_(nullptr)
    {
        memset(&master_addr_, 0, sizeof(master_addr_));
        master_addr_.sin_family = AF_INET;
    }

    void Client::setMasterAddress(const std::string&master_ip,int master_port){
        master_addr_.sin_port=htons(master_port);

        if (inet_pton(AF_INET, master_ip.c_str(), &master_addr_.sin_addr) != 1) {
            perror("Invalid IP address");
            return ;
        }

    }

    bool Client::connectMaster()
    {
        client_=std::make_unique<dts::TCPClient>(
            inet_ntoa(master_addr_.sin_addr),
            ntohs(master_addr_.sin_port)
        );

        return client_->connect();
    }

    bool Client::sendToMaster(const Message&msg){
        if(!client_) return false;
        Connection* conn=client_->getConnection();
        if(!conn) return false;
        return conn->sendMessage(msg);
    }

    std::optional<int> Client::submitTask(const TaskSubmitInfo& info){
        Message msg;
        msg.header.type = MessageType::SUBMIT_TASK;
        msg.data =Protocol::serializeTaskSubmitInfo(info);

        // 1. 向 Master 提交任务
        if (!sendToMaster(msg)) {
            std::cout<< "task submit failed!"<< std::endl;

            return std::nullopt;
        }

        // 2. 等待 Master 返回 TASK_SUBMIT_ACK
        if (!client_) {
            return std::nullopt;
        }

        Connection* conn =client_->getConnection();

        if (!conn) {
            return std::nullopt;
        }

        Message response =conn->receiveMessage();

        // 3. 校验响应类型
        if (response.header.type !=
            MessageType::TASK_SUBMIT_ACK) {

            std::cerr
                << "unexpected response type: "
                << Protocol::messageTypeToString(response.header.type)
                << std::endl;

            return std::nullopt;
        }

        // 4. 解析 ACK，获得 Master 分配的 task_id
        TaskSubmitAckInfo ack =Protocol::deserializeTaskSubmitAckInfo(response.data);

        return ack.task_id;
    }
    void Client::stop(){
        if(!client_){
            return ;
        }
        Connection*conn=client_->getConnection();
        if(conn){
            conn->disconnect();
        }
    }
}
