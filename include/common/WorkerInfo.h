/*
 * WorkerInfo.h
 * Worker 信息类，包含 Worker ID、运行任务数、心跳时间、存活状态
 */
#pragma once

#include<string>
#include<chrono>
#include"utils/Config.h"
namespace dts{

class WorkerInfo{
    
private:
    int worker_id_;
    std::string worker_ip_;
    int worker_port_;
    size_t running_task_count_;
    std::chrono::system_clock::time_point  last_heartbeat_time_;//只记录最后一次Heartbeat
    bool alive_;

public:
    WorkerInfo(int workerId,const std::string& workerIp,int workerPort);
    int getWorkerId()const;
    const std::string&getIp()const;
    int getPort()const;
    std::chrono::system_clock::time_point getLastHeartbeatTime()const;

    size_t getRunningTaskCount() const;
    void setRunningTaskCount(size_t count);
    void updateHeartbeat();

    void incrementLoad();
    void decrementLoad();
    bool isOverTime(int timeoutSeconds)const ;
    bool isAlive() const;
    void markDead();

};

}