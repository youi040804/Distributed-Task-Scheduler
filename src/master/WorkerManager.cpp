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
    void WorkerManager::addWorker(WorkerInfo&& worker, std::shared_ptr<Connection> conn){
        std::lock_guard<std::mutex> lock(worker_mutex_);
        workers_.emplace(worker.getWorkerId(), WorkerSession{std::move(worker), conn});
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
            return it->second.info;
        }
        return std::nullopt;;
    }

    bool WorkerManager::updateWorkerHeartbeat(int workerId) {
        std::lock_guard<std::mutex>lock(worker_mutex_);
        auto it = workers_.find(workerId);
        if (it == workers_.end()) {
            return false;
        }
        it->second.info.updateHeartbeat();
        return true;
    }
    bool WorkerManager::updateWorkerLoad(int workerId, size_t runningCount, size_t queuedCount){
        std::lock_guard<std::mutex>lock(worker_mutex_);
        
        auto it=workers_.find(workerId);
        if (it== workers_.end()) {
            std::cout<<"worker not exist! Update task count failed!"<<std::endl;
            return false; 
        }

        it->second.info.setRunningTaskCount(runningCount);
        it->second.info.setQueuedTaskCount(queuedCount);
        return true;
    }

    //获取超时的worker
    std::vector<int> WorkerManager::getTimeoutWorker(){
        std::vector<int>timeoutList;

        std::lock_guard<std::mutex>lock(worker_mutex_);

        for(const auto& worker:workers_){
            if(worker.second.info.isOverTime(HEARTBEAT_TIMEOUT))
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
        it->second.info.markDead();
        return true;
    }

    // Least Load 调度策略：选择当前任务负载最小的 Worker
    // Load = running_task_count + queued_task_count
    std::pair<int,size_t>WorkerManager::pickLeastLoadedWorker(){
        std::lock_guard<std::mutex> lock(worker_mutex_);
        int workerId=-1;
        size_t LeastLoad=SIZE_MAX;
        if(workers_.empty()){
            return {workerId,0};
        }


        for (const auto& worker : workers_) {
            if (worker.second.info.isAlive()) {
                size_t load = worker.second.info.getWorkerLoad();
                // 使用 pair 比较：先比负载，再比 ID
                if (std::pair<size_t, int>{load, worker.first} < 
                    std::pair<size_t, int>{LeastLoad, workerId}) {
                    LeastLoad = load;
                    workerId = worker.first;
                }
            }
        }

        // 没有可用 Worker 时，负载返回 0
        if (workerId == -1) {
            return {workerId, 0};
        }
        return {workerId,LeastLoad};

    }
    bool WorkerManager::sendTaskToWorker(int workerId,Message&msg){
        std::shared_ptr<Connection>conn;
        
        // 锁外发送
        // 1. 锁住，拿到 connection
        {
            std::lock_guard<std::mutex> lock(worker_mutex_);
            auto it=workers_.find(workerId);
            if(it==workers_.end()){
                return false;
            }
            if(!it->second.info.isAlive()){
                return false;
            }
            
            conn = it->second.connection;  // 复制 shared_ptr
        }  // ← 释放锁！
        
      
        return conn->sendMessage(msg);
    }



} 
