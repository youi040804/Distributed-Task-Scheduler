/*
 * Worker.h
 * Worker 节点类，负责连接 Master、注册自身、执行任务
 */
#pragma once
#include<sys/socket.h>
#include<memory>
#include<string>
#include <atomic>
#include<thread>
#include<queue>
#include<mutex>
#include"network/TCPClient.h"
#include "common/Message.h"  
#include"common/Protocol.h"
#include "utils/Config.h"
#include"TaskExecutor.h"
namespace dts{

class Worker{
private:
    int worker_id_;
    sockaddr_in master_addr_ ;
    std::atomic<bool> running_;
    std::unique_ptr<TCPClient>worker_client_;
    std::atomic<size_t> running_task_count_; 
    std::atomic<size_t> queued_task_count_; 
     
    std::thread heartbeat_thread_;
    std::thread task_recv_thread_;
    std::thread task_execute_thread_;

    std::queue<TaskAssignInfo>task_queue_;
    std::unique_ptr<TaskExecutor>executor_;
    std::mutex task_mutex_;


public:
    explicit Worker(int worker_id);
    void setMasterAddress(const std::string& master_ip, int master_port);
    bool connectMaster();

    bool start(const std::string& master_ip, int master_port,const std::string& worker_ip, int worker_port); 
    void startThreads();
    bool sendToMaster(Message& msg); 

    void incrementRunningTasks();
    void decrementRunningTasks();
    
    //暂且用sendHeartbeat()函数实现发送一次心跳消息，先不实现定时循环发送心跳消息
    bool sendHeartbeat();
    void heartbeatLoop();
    void receiveTaskLoop();
    void executeTaskLoop();
    Message recvTaskAssign();

    void stop();
};
}