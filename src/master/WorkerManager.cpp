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
    workers_.emplace(worker.getWorkerId(),std::move(worker));
}
// WorkerManager.h
const WorkerInfo* WorkerManager::getWorkerInfo(int workerId) const {
    auto it = workers_.find(workerId);
    if (it != workers_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool WorkerManager::updateWorkerHeartbeat(int workerId) {
    auto it = workers_.find(workerId);
    if (it == workers_.end()) {
        return false;
    }
    it->second.updateHeartbeat();
    return true;
}
bool WorkerManager::updateWorkerTaskCount(int workerId, size_t taskCount){
    auto it=workers_.find(workerId);
    if (it== workers_.end()) {
        std::cout<<"worker not exist! Update task count failed!"<<std::endl;
        return false; 
    }
    it->second.setRunningTaskCount(taskCount);
    return true;
}

bool WorkerManager::markWorkerDead(int workerId){
    auto it=workers_.find(workerId);
    if (it== workers_.end()) {
        std::cout<<"worker not exist!"<<std::endl;
        return false;
    }
    it->second.markDead();
    return true;
}


} 
