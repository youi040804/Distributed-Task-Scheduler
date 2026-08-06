/*
 * Worker.cpp
 * Worker 类的实现，包含连接 Master、注册自身等逻辑
 */
#include <arpa/inet.h> 
#include <cstring> 
#include <thread>
#include"worker/Worker.h"
#include"common/WorkerInfo.h"
#include"common/Protocol.h"


namespace dts{

    Worker::Worker(int worker_id)
        : worker_id_(worker_id),
        worker_client_(nullptr)
    {
        memset(&master_addr_, 0, sizeof(master_addr_));
        master_addr_.sin_family = AF_INET;
    }
    
    void Worker::setMasterAddress(const std::string&master_ip,int master_port){
        master_addr_.sin_port=htons(master_port);

        if (inet_pton(AF_INET, master_ip.c_str(), &master_addr_.sin_addr) != 1) {
            perror("Invalid IP address");
            return ;
        }

    }

    bool Worker::connectMaster()
    {
        worker_client_=std::make_unique<dts::TCPClient>(
            inet_ntoa(master_addr_.sin_addr),
            ntohs(master_addr_.sin_port)
        );

        return worker_client_->connect();    
    }

    //由原来的发送字符串改为发送真正的Message
    bool Worker::sendToMaster(Message&msg){
        Connection* conn=worker_client_->getConnection();
        return conn->sendMessage(msg);
    }
    
    bool Worker::start(const std::string& master_ip, int master_port,
                       const std::string& worker_ip, int worker_port){
        //1.连接Master
        this->setMasterAddress(master_ip,master_port);
        int connect_result=this->connectMaster();
        if(!connect_result) {
            perror("worker connect failed!");
            return false;
        }
        // 2. 构造注册消息（用传入的参数）
        WorkerRegisterInfo info;
        info.worker_id = worker_id_;
        info.ip = worker_ip;      
        info.port = worker_port;  

        //3.发送注册消息
        Message msg;
        msg.header.type=MessageType::REGISTER_WORKER;
        msg.data=Protocol::serializeWorkerInfo(info);

        if(!sendToMaster(msg)){
            perror("worker start failed!");
            return false;
        }

        running_=true;
        return true;
    }


    void Worker::incrementRunningTasks() { 
        running_task_count_++; 
    }
    void Worker::decrementRunningTasks() {
         if (running_task_count_ > 0) 
         running_task_count_--; 
    }

    bool Worker::sendHeartbeat(){
        // 1. 构造心跳消息
        HeartbeatInfo info;
        info.worker_id=worker_id_;
        info.running_task_count=running_task_count_.load();
        //先实现只发送一次心跳消息
        Message msg;
        msg.header.type=MessageType::HEARTBEAT;
        msg.data=Protocol::serializeHeartbeatInfo(info);
        return sendToMaster(msg);
    }

}