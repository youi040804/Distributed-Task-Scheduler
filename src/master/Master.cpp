/*
 * Master.cpp
 * Master 类的实现，包含主循环、消息分发和 Worker 注册处理
 */
#include<iostream>
#include"master/Master.h"
#include "common/Protocol.h"  

namespace dts{

Master::Master(int master_port):port_(master_port), running_(false){
}

bool Master::start(){
    master_server_=std::make_shared<TCPServer>(port_);

    int master_start_result=master_server_->start();
    if(!master_start_result){
        perror("master start failed");
        return false;
    }
    running_=true;
    return true;
}

void Master::handleWorkerRegister(const WorkerRegisterInfo&RegisterInfo){
    if(worker_manager_.getWorkerInfo(RegisterInfo.worker_id)!=nullptr) 
    {
        std::cout<<"worker already exists!"<<std::endl;
        return ;//如果worker存在直接返回
    }
    //不存在则插入
    WorkerInfo worker(RegisterInfo.worker_id,RegisterInfo.ip,RegisterInfo.port);
    worker_manager_.addWorker(std::move(worker));
    std::cout<<"worker added succeed!"<<std::endl;

}

bool Master::handleHeartbeat(const HeartbeatInfo& info) {
    if (!worker_manager_.updateWorkerHeartbeat(info.worker_id)) {
        return false;
    }
    return worker_manager_.updateWorkerTaskCount(info.worker_id, info.running_task_count);
}

void Master::run() {
    while (running_) {
        auto conn = master_server_->acceptConnection();

        if (!conn) {
            continue;
        }

        while (running_) {
            Message msg = conn->receiveMessage();
            if (msg.header.type == MessageType::UNKNOWN && msg.data.empty()) {
                break;
            }

            switch (msg.header.type) {
                case MessageType::REGISTER_WORKER: {
                    WorkerRegisterInfo workerinfo = Protocol::deserializeWorkerInfo(msg.data);
                    handleWorkerRegister(workerinfo);
                    break;
                }
                case MessageType::HEARTBEAT: {
                    HeartbeatInfo info = Protocol::deserializeHeartbeatInfo(msg.data);
                    handleHeartbeat(info);
                    break;
                }
                default: {
                    break;
                }
            }
        }
    }
}
const WorkerInfo* Master::getWorkerInfo(int workerId) const {
    return worker_manager_.getWorkerInfo(workerId);
}

}
