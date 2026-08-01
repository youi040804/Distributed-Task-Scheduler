/*
 * Master.h
 * Master 节点主控类，负责接收 Worker 注册、管理 Worker、调度任务
 */

#pragma once
#include<thread>
#include"network/TCPServer.h"
#include"common/WorkerInfo.h"
#include "WorkerManager.h"
namespace dts{
struct WorkerRegisterInfo;
struct HeartbeatInfo;


class Master{
private:
int port_;
bool running_;
std::shared_ptr<TCPServer>master_server_;

std::thread heartbeat_thread_;
void heartbeatLoop();

WorkerManager worker_manager_;


public:
Master(int master_port);
bool start();
void handleWorkerRegister(const WorkerRegisterInfo&workerinfo);
bool handleHeartbeat(const HeartbeatInfo& info);

void run();
void stop();

const WorkerInfo* getWorkerInfo(int workerId) const;
};

}