/*
 * Master.h
 * Master 节点主控类，负责接收 Worker 注册、管理 Worker、调度任务
 */

#pragma once
#include<thread>
#include<atomic>
#include<memory>
#include"network/TCPServer.h"
#include"common/WorkerInfo.h"
#include "WorkerManager.h"
#include "utils/Config.h"
#include"common/Protocol.h"
#include"master/TaskManager.h"
#include"Scheduler.h"
namespace dts{
struct WorkerRegisterInfo;
struct HeartbeatInfo;


class Master{
private:
int port_;
std::atomic<bool> running_;
std::shared_ptr<TCPServer>master_server_;

std::thread heartbeat_thread_;
//任务调度线程
std::thread scheduler_thread_;

void heartbeatLoop();

WorkerManager worker_manager_;
TaskManager task_manager_;
std::unique_ptr<Scheduler>scheduler_;

std::atomic<int>next_id_{1};//Task ID生成器


public:
Master(int master_port);
~Master();
bool start();
void handleWorkerRegister(const WorkerRegisterInfo&workerinfo);
bool handleHeartbeat(const HeartbeatInfo& info);

void handleTaskSubmit(const TaskSubmitInfo&info);

void schedulerLoop();

// 转发给 WorkerManager（内联实现）

std::optional<WorkerInfo> getWorkerInfo(int workerId) const {
    return worker_manager_.getWorkerInfo(workerId);
}

void run();
void stop();

};

}