/*
 * WorkerManager.h
 * Worker 管理器，维护所有已注册 Worker 的信息和状态
 */
#pragma once
#include"common/WorkerInfo.h"
#include<unordered_map>
#include <utility>  
#include<mutex>
#include<optional>

namespace dts{
class WorkerManager{
private:
    std::unordered_map<int,WorkerInfo> workers_;//key:workerId,value:WorkerInfo
    mutable std::mutex worker_mutex_;

public:
    void addWorker(WorkerInfo&& worker);
    bool  hasWorker(int workerId)const;

    std::optional<WorkerInfo> getWorkerInfo(int workerId) const;

    bool updateWorkerHeartbeat(int workerId);
    bool updateWorkerTaskCount(int workerId, size_t taskCount);

    //获取超时的worker
    std::vector<int> getTimeoutWorker();
    bool markWorkerDead(int workerId);
    bool increaseWorkerTaskCount(int workerId);
    std::pair<int,size_t> pickLeastLoadedWorker();

};


}