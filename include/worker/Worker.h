/*
 * Worker.h
 * Worker 节点类，负责连接 Master、注册自身、执行任务
 */
#pragma once
#include<sys/socket.h>
#include<memory>
#include<string>
#include <atomic>
#include"network/TCPClient.h"
#include "common/Message.h"  
#include "utils/Config.h"
namespace dts{

class Worker{
private:
    int worker_id_;
    sockaddr_in master_addr_ ;
    bool running_;
    std::unique_ptr<TCPClient>worker_client_;
    bool sendToMaster(Message& msg); 
    std::atomic<size_t> running_task_count_; 

public:
    explicit Worker(int worker_id);
    void setMasterAddress(const std::string& master_ip, int master_port);
    bool connectMaster();

    bool start(const std::string& master_ip, int master_port,const std::string& worker_ip, int worker_port); 

    void incrementRunningTasks();
    void decrementRunningTasks();
    
    //暂且用sendHeartbeat()函数实现发送一次心跳消息，先不实现定时循环发送心跳消息
    bool sendHeartbeat();

};
}