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
                    TaskSubmitInfo info = Protocol::deserializeTaskSubmitInfo(msg.data);
                    handleTaskSubmit(info);
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
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(HEARTBEAT_CHECK_INTERVAL));
            auto timeoutList = worker_manager_.getTimeoutWorker();
            for (int id : timeoutList) {
                worker_manager_.markWorkerDead(id);
            }
        }
    }

    void Master::handleTaskSubmit(const TaskSubmitInfo&info){
        // TODO: 当前是单向通知，Client无法获知task_id
        // 后续需要改为请求-响应模型，返回任务ID
        //1.创建Task
        int id=next_id_.fetch_add(1);//原子递增
        Task task(id,info.priority,info.payload);
        //2.将Task保存到taskmanager里
        task_manager_.addTask(std::move(task));
    }

    bool Master::handleTaskResult(const TaskResultInfo&info){
        //先假设所有任务的返回结果都是成功的
        //Master不负责更改任务状态，交由TaskManager来更新任务状态
        //只处理 Task 状态，不修改 Worker 负载
        auto worker_id=task_manager_.processTaskResult(info.task_id,info.payload,info.status);
        
        if(!worker_id.has_value())
        {
            std::cout<<"task process failed"<<std::endl;
            return false;
        }

        // 不修改 WorkerManager 的负载
        // 等待下一次 Heartbeat 来同步真实负载
        return true;

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
