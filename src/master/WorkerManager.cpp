/*
 * WorkerManager.cpp
 * WorkerManager 类的实现，管理 Worker 的增删查改
 */
#include"../../include/master/WorkerManager.h"
#include<utility>
namespace dts
{
void WorkerManager::addWorker(WorkerInfo&&worker){
    workers_.emplace(worker.getWorkerId(),std::move(worker));
}
bool WorkerManager::IsWorkerExist(int WorkerId){
    if(workers_.count(WorkerId)>0)
    return true;
    else return false;
}
// WorkerManager.h
const WorkerInfo* WorkerManager::getWorkerInfo(int workerId) const {
    auto it = workers_.find(workerId);
    if (it != workers_.end()) {
        return &it->second;
    }
    return nullptr;
}

} 
