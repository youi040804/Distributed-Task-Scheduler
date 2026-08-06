/*
 * Master.cpp
 * Master 类的实现，包含主循环、消息分发和 Worker 注册处理
 */
#include<iostream>
#include<chrono>
#include"master/Master.h"

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
    scheduler_=std::make_unique<Scheduler>(&task_manager_,&worker_manager_);
    //创建心跳检测线程
    heartbeat_thread_=std::thread(&Master::heartbeatLoop,this);
    //创建任务调度线程
    scheduler_thread_=std::thread(&Master::schedulerLoop,this);
    return true;

}

void Master::handleWorkerRegister(const WorkerRegisterInfo&RegisterInfo){
    if(worker_manager_. hasWorker(RegisterInfo.worker_id))
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

void Master::heartbeatLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(HEARTBEAT_CHECK_INTERVAL));
        auto timeoutList = worker_manager_.getTimeoutWorker();
        for (int id : timeoutList) {
            worker_manager_.markWorkerDead(id);
        }
    }
}

void Master::handleTaskSubmit(const TaskSubmitInfo&info){
        //1.创建Task
        int id=next_id_.fetch_add(1);//原子递增
        Task task(id,info.priority,info.payload);
        //2.将Task保存到taskmanager里
        task_manager_.addTask(std::move(task));
}

void Master::schedulerLoop(){

    while(running_){
        scheduler_->schedulerOnce();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
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
                case MessageType::SUBMIT_TASK: {
                    TaskSubmitInfo info = Protocol::deserializeTaskSubmitInfo(msg.data);
                    handleTaskSubmit(info);
                    break;
                }
                default: {
                    break;
                }
            }
        }
    }
}


void Master::stop(){
    running_=false;
    if(heartbeat_thread_.joinable()){
        heartbeat_thread_.join();

    }

}

Master::~Master() {
    stop();
}

}
