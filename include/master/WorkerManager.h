/*
 * WorkerManager.h
 * Worker 管理器，维护所有已注册 Worker 的信息和状态
 */
#pragma once

#include"../common/WorkerInfo.h"
#include<unordered_map>
namespace dts{
class WorkerManager{
private:
std::unordered_map<int,WorkerInfo> workers_;//key:workerId,value:WorkerInfo
public:
void addWorker(WorkerInfo&& worker);
bool IsWorkerExist(int WorkerId);

const WorkerInfo* getWorkerInfo(int workerId) const;

};


}