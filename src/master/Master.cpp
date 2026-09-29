/*
 * Master.cpp
 * Master 类的实现，包含主循环、消息分发和 Worker 注册处理
 */
#include<iostream>
#include<chrono>
#include"master/Master.h"

namespace dts{
    Master::Master(int master_port)
        : port_(master_port)
        , running_(false)
    {
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

    void Master::handleConnection(std::shared_ptr<Connection>conn){
        //std::thread把参数conn传递给handleConnection函数
        //等待收到消息
        while (running_) {
            Message msg = conn->receiveMessage();
            if (msg.header.type == MessageType::UNKNOWN && msg.data.empty()) {
                break;
            }

            switch (msg.header.type) {
                case MessageType::REGISTER_WORKER: {
                    WorkerRegisterInfo workerinfo = Protocol::deserializeWorkerInfo(msg.data);
                    handleWorkerRegister(workerinfo,conn);
                    break;
                }
                case MessageType::HEARTBEAT: {
                    HeartbeatInfo info = Protocol::deserializeHeartbeatInfo(msg.data);
                    handleHeartbeat(info);
                    break;
                }

                case MessageType::SUBMIT_TASK: {
                    TaskSubmitInfo info =Protocol::deserializeTaskSubmitInfo(msg.data);

                    const int task_id =handleTaskSubmit(info);

                    TaskSubmitAckInfo ack_info;
                    ack_info.task_id = task_id;

                    Message ack_msg;
                    ack_msg.header.type =MessageType::TASK_SUBMIT_ACK;

                    ack_msg.data =Protocol::serializeTaskSubmitAckInfo(ack_info);

                    if (!conn->sendMessage(ack_msg)) {
                        std::cerr
                            << "[Master] Failed to send "
                            << "TASK_SUBMIT_ACK for Task "
                            << task_id
                            << std::endl;
                    }

                    break;
                }
                case MessageType::QUERY_TASK: {
                    TaskQueryInfo query_info =Protocol::deserializeTaskQueryInfo(msg.data);

                    TaskStatusInfo status_info =handleTaskQuery(query_info);

                    Message status_msg;
                    status_msg.header.type = MessageType::TASK_STATUS;
                    status_msg.data =Protocol::serializeTaskStatusInfo(status_info);

                    if (!conn->sendMessage(status_msg)) {
                        std::cerr
                            << "[Master] Failed to send TASK_STATUS for Task "
                            << query_info.task_id
                            << std::endl;
                    }

                    break;
                }
                case MessageType::TASK_RESULT: {
                    TaskResultInfo info = Protocol::deserializeTaskResultInfo(msg.data);
                    handleTaskResult(info);
                    break;
                }
                default: {
                    break;
                }
            }
        }
        //退出运行时调用Connection类的disconnect()
        conn->disconnect();
    }

    void Master::handleWorkerRegister(const WorkerRegisterInfo&RegisterInfo,std::shared_ptr<Connection>conn){
        if(worker_manager_. hasWorker(RegisterInfo.worker_id))
        {
            std::cout<<"worker already exists!"<<std::endl;
            return ;//如果worker存在直接返回
        }
        //不存在则插入
        WorkerInfo worker(RegisterInfo.worker_id,RegisterInfo.ip,RegisterInfo.port);
        worker_manager_.addWorker(std::move(worker),conn);
        std::cout<<"worker added succeed!"<<std::endl;
    }

    bool Master::handleHeartbeat(const HeartbeatInfo& info) {
        // 1. 更新心跳时间
        if (!worker_manager_.updateWorkerHeartbeat(info.worker_id)) {
            return false;
        }
        // 2. 同步 Worker 真实负载（由 Heartbeat 上报）
        return worker_manager_.updateWorkerLoad(info.worker_id,info.running_task_count,info.queued_task_count);
    }

    void Master::heartbeatLoop() {
        std::unique_lock<std::mutex>lock(heartbeat_mutex_);
        while (running_) {
            const bool stopRequested=heartbeat_cv_.wait_for(
                lock,
                std::chrono::seconds(HEARTBEAT_CHECK_INTERVAL),
                [this](){
                    return !running_;
                }
            );
            
            if(stopRequested){
                break;
            }
            lock.unlock();
            auto timeoutList = worker_manager_.getTimeoutWorker();
            for (int id : timeoutList) {
                if(worker_manager_.markWorkerDead(id)){
                    const size_t recoveredCount=task_manager_.recoverTasksForWorker(id);
                    std::cout << "[Master] Worker " << id
                              << " timed out, recovered "
                              << recoveredCount << " running task(s)"
                              << std::endl;
                }

            }
            lock.lock();
        }
    }


    int Master::handleTaskSubmit(const TaskSubmitInfo& info){
        int id = next_id_.fetch_add(1);

        Task task(id,info.priority,info.payload);

        task_manager_.addTask(std::move(task));

        return id;
    }
    TaskStatusInfo Master::handleTaskQuery(const TaskQueryInfo& info) {
        TaskStatusInfo status_info;
        status_info.task_id = info.task_id;

        auto snapshot =task_manager_.getTaskSnapshot(info.task_id);

        if (!snapshot.has_value()) {
            status_info.found = false;
            return status_info;
        }

        status_info.found = true;
        status_info.status = snapshot->status;
        status_info.result = snapshot->result;

        return status_info;
    }

    bool Master::handleTaskResult(const TaskResultInfo& info){
        //Master不负责更改任务状态，交由TaskManager来更新任务状态
        auto result = task_manager_.processTaskResult(info.task_id,info.execution_id,
                                                        info.payload,info.status);
        
        switch(result.code){

            case ProcessResultCode::SUCCESS:{
                std::cout << "[Master] Task "<< info.task_id<< " completed successfully"<< std::endl;
                return true;
            }
            case ProcessResultCode::RETRY:{
                std::cout << "[Master] Task "<< info.task_id<< " failed, retry scheduled"<< std::endl;
                return true;
            }
            case ProcessResultCode::FINAL_FAILED:{
                std::cout << "[Master] Task "<< info.task_id<< " failed permanently"<< std::endl;
                return true;
            }
            case ProcessResultCode::NOT_FOUND:{
                std::cout << "[Master] Task "<< info.task_id<< " not found"<< std::endl;
                return false;
            }
            case ProcessResultCode::INVALID_TRANSITION:{
                std::cout << "[Master] Task "<< info.task_id<< " invalid status transition"<< std::endl;
                return false;
            }
            case ProcessResultCode::STALE_EXECUTION: {
                std::cout<< "[Master] Ignore stale result for Task "<< info.task_id
                            << ", execution="<< info.execution_id<< std::endl;

                return false;
            }
        }
        // 不修改 WorkerManager 的负载,等待下一次 Heartbeat 来同步真实负载
        return false;
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
            //Connection有效，创建一个线程
            std::thread(&Master::handleConnection,this,conn).detach();//用 std::thread::detach() + 用 running_ 控制退出

        }
    }

    void Master::stop(){
        running_=false;
        heartbeat_cv_.notify_all();
        //关闭监听socket，唤醒阻塞在accept()的Master主线程
        if(master_server_){
            master_server_->stop();
        }
        if(heartbeat_thread_.joinable()){
            heartbeat_thread_.join();
        }
        if(scheduler_thread_.joinable()){
            scheduler_thread_.join();
        }
    }

    Master::~Master() {
        stop();
    }

}
