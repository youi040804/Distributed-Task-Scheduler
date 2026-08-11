/*
 * Worker.h
 * Worker 节点类，负责连接 Master、注册自身、执行任务
 */
#pragma once
#include<sys/socket.h>
#include<memory>
#include<string>
#include <atomic>
#include<queue>
#include<thread>
#include<mutex>
#include <condition_variable>
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
    //条件变量，用于生产者-消费者模型
    std::condition_variable task_cv_;


public:
    explicit Worker(int worker_id);
    void setMasterAddress(const std::string& master_ip, int master_port);
    bool connectMaster();

    bool start(const std::string& master_ip, int master_port,const std::string& worker_ip, int worker_port); 
    void startThreads();
    bool sendToMaster(const Message& msg); 
  
    bool sendHeartbeat();
    void heartbeatLoop();
    void receiveTaskLoop();
    void executeTaskLoop();
    Message recvTaskAssign();

    void stop();
};
}