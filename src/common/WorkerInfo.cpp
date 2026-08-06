/*
 * WorkerInfo.cpp
 * WorkerInfo 类的实现，管理单个 Worker 的状态信息
 */
#include"common/WorkerInfo.h"
#include<iostream>
namespace dts{
    WorkerInfo::WorkerInfo(int workerId, const std::string& workerIp, int workerPort)
        : worker_id_(workerId)
        , worker_ip_(workerIp)
        , worker_port_(workerPort)
        , running_task_count_(0)
        , last_heartbeat_time_(std::chrono::system_clock::now())
        , alive_(true)
    {
    }

    int WorkerInfo::getWorkerId()const{
        return worker_id_;
    }
    const std::string& WorkerInfo::getIp()const{
        return worker_ip_;
    }
    
    int WorkerInfo::getPort()const{
        return worker_port_;
    }
    size_t WorkerInfo::getRunningTaskCount()const{
        return running_task_count_;
    }
    void WorkerInfo::setRunningTaskCount(size_t count){
        running_task_count_=count;
    }
    void WorkerInfo::increaseRunningTaskCount(){
        running_task_count_++;
    }
    void WorkerInfo::decreaseRunningTaskCount(){
        if (running_task_count_ > 0) {
            running_task_count_--;
        }
    }
    std::chrono::system_clock::time_point WorkerInfo::getLastHeartbeatTime()const{
        return last_heartbeat_time_;
    }

    void WorkerInfo::updateHeartbeat(){
        //更新last_heartbeat_time_
        last_heartbeat_time_=std::chrono::system_clock::now();
        alive_=true;
    }

    bool WorkerInfo::isOverTime(int timeoutSeconds)const {
        auto now=std::chrono::system_clock::now();
        return now-last_heartbeat_time_>std::chrono::seconds(timeoutSeconds);
    }

    bool WorkerInfo::isAlive() const{
        return alive_;
    }
    void WorkerInfo::markDead(){
        alive_=false;
    }

}