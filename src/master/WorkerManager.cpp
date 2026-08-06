/*
 * WorkerManager.cpp
 * WorkerManager 类的实现，管理 Worker 的增删查改
 */
#include<utility>
#include<iostream>
#include<vector>
#include"master/WorkerManager.h"

namespace dts
{
void WorkerManager::addWorker(WorkerInfo&&worker){
    std::lock_guard<std::mutex> lock(worker_mutex_);
    workers_.emplace(worker.getWorkerId(),std::move(worker));
}

bool WorkerManager:: hasWorker(int workerId)const {
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
bool WorkerManager::increaseWorkerTaskCount(int workerId) {
    std::lock_guard<std::mutex> lock(worker_mutex_);
    auto it = workers_.find(workerId);
    if (it == workers_.end()) {
        return false;
    }
    it->second.increaseRunningTaskCount();
    return true;
}

//Least Load 调度策略：选择当前运行任务最少的worker
std::pair<int,size_t>WorkerManager::pickLeastLoadedWorker(){
    int workerId=-1;
    size_t LeastLoad=SIZE_MAX;
    if(workers_.empty()){
        return {workerId,0};
    }

    for(const auto&worker:workers_){
        if(worker.second.isAlive()){
            size_t load=worker.second.getRunningTaskCount();
            if(load<LeastLoad){
                LeastLoad=load;
                workerId=worker.second.getWorkerId();
            }
        }
      
    }
    
    // 没有可用 Worker 时，负载返回 0
    if (workerId == -1) {
        return {workerId, 0};
    }
    return {workerId,LeastLoad};

}


} 
