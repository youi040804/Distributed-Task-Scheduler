/*
 * WorkerInfo.cpp
 * WorkerInfo 类的实现，管理单个 Worker 的状态信息
 */
#include"../../include/common/WorkerInfo.h"

namespace dts{

WorkerInfo::WorkerInfo(int workerId, const std::string& workerIp,int workerPort)
    :worker_id_(workerId),
    worker_ip_(workerIp),
    worker_port_(workerPort),
    running_task_count_(0),
    last_heartbeat_time_(std::chrono::system_clock::now()),
    alive_(true){

}

int WorkerInfo::getWorkerId()const{
    return worker_id_;
}
std::string WorkerInfo::getIp()const{
    return worker_ip_;
}
int WorkerInfo::getPort()const{
    return worker_port_;
}
size_t WorkerInfo::getRunningTaskCount()const{
    return running_task_count_;
}

void WorkerInfo::updateHeartbeat(){
    //更新last_heartbeat_time_
}
void WorkerInfo::increaseTaskCount(){
    running_task_count_++;
}
void WorkerInfo::decreaseTaskCount(){
    if (running_task_count_ > 0) {
        running_task_count_--;
    }
}
bool WorkerInfo::isAlive() const{
    return alive_;
}
void WorkerInfo::markDead(){
    alive_=false;
}

}