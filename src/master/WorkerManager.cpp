/*
 * WorkerManager.cpp
 * WorkerManager 类的实现，管理 Worker 的增删查改
 */
#include<utility>
#include<iostream>
#include"master/WorkerManager.h"

namespace dts
{
void WorkerManager::addWorker(WorkerInfo&&worker){
    std::lock_guard<std::mutex> lock(worker_mutex_);
    workers_.emplace(worker.getWorkerId(),std::move(worker));
}
bool WorkerManager::hasWorker(int workerId)const {
    std::lock_guard<std::mutex>lock(worker_mutex_);
    auto it = workers_.find(workerId);
    if (it!= workers_.end()) {
        return true;
    }
    return false;
}
// WorkerManager.h
std::optional<WorkerInfo> WorkerManager::getWorkerInfo(int workerId) const {

    std::lock_guard<std::mutex> lock(worker_mutex_);
    auto it = workers_.find(workerId);
    if (it != workers_.end()) {
        return it->second;
    }
     return std::nullopt;;
}

bool WorkerManager::updateWorkerHeartbeat(int workerId) {
    std::lock_guard<std::mutex>lock(worker_mutex_);
    auto it = workers_.find(workerId);
    if (it == workers_.end()) {
        return false;
    }
    it->second.updateHeartbeat();
    return true;
}
bool WorkerManager::updateWorkerTaskCount(int workerId, size_t taskCount){
    std::lock_guard<std::mutex>lock(worker_mutex_);
    
    auto it=workers_.find(workerId);
    if (it== workers_.end()) {
        std::cout<<"worker not exist! Update task count failed!"<<std::endl;
        return false; 
    }
    it->second.setRunningTaskCount(taskCount);
    return true;
}

//获取超时的worker
std::vector<int> WorkerManager::getTimeoutWorker(){
    std::vector<int>timeoutList;

    std::lock_guard<std::mutex>lock(worker_mutex_);

    for(const auto& worker:workers_){
        if(worker.second.isOverTime(HEARTBEAT_TIMEOUT))
        timeoutList.push_back(worker.first);//push back workerID into timeoutList
    }
    return timeoutList;
}

bool WorkerManager::markWorkerDead(int workerId){
    std::lock_guard<std::mutex> lock(worker_mutex_);
    auto it=workers_.find(workerId);
    if (it== workers_.end()) {
        std::cout<<"worker not exist!"<<std::endl;
        return false;
    }
    it->second.markDead();
    return true;
}


} 
