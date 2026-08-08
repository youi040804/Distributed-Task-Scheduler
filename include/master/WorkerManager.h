/*
 * WorkerManager.h
 * Worker 管理器，维护所有已注册 Worker 的信息和状态
 */
#pragma once
#include<unordered_map>
#include <utility>  
#include<mutex>
#include<optional>
#include<memory>
#include<vector>
#include"common/WorkerInfo.h"
#include"common/Message.h"
#include"network/Connection.h"

namespace dts{
struct WorkerSession{
    WorkerInfo info;
    std::shared_ptr<Connection> connection;
};
class WorkerManager{
private:
    std::unordered_map<int,WorkerSession> workers_;//key:workerId,value:WorkerSession
    mutable std::mutex worker_mutex_;

public:
    void addWorker(WorkerInfo&& worker, std::shared_ptr<Connection> conn);
    bool hasWorker(int workerId)const;

    std::optional<WorkerInfo> getWorkerInfo(int workerId) const;

    bool updateWorkerHeartbeat(int workerId);
    bool updateWorkerLoad(int workerId, size_t runningCount, size_t queuedCount);

    //新增
    bool incrementWorkerQueuedTaskCount(int workerId);
    
    //获取超时的worker
    std::vector<int> getTimeoutWorker();
    bool markWorkerDead(int workerId);
    std::pair<int,size_t> pickLeastLoadedWorker();
    bool sendTaskToWorker(int workerId,Message&msg);
};


}