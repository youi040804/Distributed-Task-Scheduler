/*
 * Worker.cpp
 * Worker 类的实现，包含连接 Master、注册自身等逻辑
 */
#include <arpa/inet.h>
#include <cstring>
#include <thread>
#include"worker/Worker.h"
#include"common/WorkerInfo.h"


namespace dts{

    Worker::Worker(int worker_id)
        : worker_id_(worker_id),
        worker_client_(nullptr)
    {
        memset(&master_addr_, 0, sizeof(master_addr_));
        master_addr_.sin_family = AF_INET;
    }

    void Worker::setMasterAddress(const std::string&master_ip,int master_port){
        master_addr_.sin_port=htons(master_port);

        if (inet_pton(AF_INET, master_ip.c_str(), &master_addr_.sin_addr) != 1) {
            perror("Invalid IP address");
            return ;
        }

    }

    bool Worker::connectMaster()
    {
        worker_client_=std::make_unique<dts::TCPClient>(
            inet_ntoa(master_addr_.sin_addr),
            ntohs(master_addr_.sin_port)
        );

        return worker_client_->connect();
    }

    //由原来的发送字符串改为发送真正的Message
    bool Worker::sendToMaster(Message&msg){
        Connection* conn=worker_client_->getConnection();
        return conn->sendMessage(msg);
    }

    bool Worker::start(const std::string& master_ip, int master_port,
                       const std::string& worker_ip, int worker_port){
        //1.连接Master
        this->setMasterAddress(master_ip,master_port);
        int connect_result=this->connectMaster();
        if(!connect_result) {
            perror("worker connect failed!");
            return false;
        }
        // 2. 构造注册消息（用传入的参数）
        WorkerRegisterInfo info;
        info.worker_id = worker_id_;
        info.ip = worker_ip;
        info.port = worker_port;

        //3.发送注册消息
        Message msg;
        msg.header.type=MessageType::REGISTER_WORKER;
        msg.data=Protocol::serializeWorkerInfo(info);

        if(!sendToMaster(msg)){
            perror("worker start failed!");
            return false;
        }

        running_=true;
        //启动所有线程
        startThreads();

        return true;
    }
    void Worker::startThreads()
    {
        //创建心跳线程
        heartbeat_thread_ =
            std::thread(&Worker::heartbeatLoop,this);
        //创建任务接收线程
        task_recv_thread_ =
            std::thread(&Worker::receiveTaskLoop,this);
        //创建任务执行线程
        task_execute_thread_ =
            std::thread(&Worker::executeTaskLoop,this);
    }

    void Worker::incrementRunningTasks() {
        running_task_count_++;
    }
    void Worker::decrementRunningTasks() {
         if (running_task_count_ > 0)
         running_task_count_--;
    }

    Message Worker::recvTaskAssign(){
       Connection* conn= worker_client_->getConnection();
       return conn->receiveMessage();
    }

    void  Worker::heartbeatLoop(){
        while (running_){
            // 1. 构造心跳消息
            HeartbeatInfo info;
            info.worker_id=worker_id_;
            info.running_task_count=running_task_count_.load();
            info.queued_task_count = queued_task_count_.load();   // ← 新增
            Message msg;
            msg.header.type=MessageType::HEARTBEAT;
            msg.data=Protocol::serializeHeartbeatInfo(info);

            std::this_thread::sleep_for(std::chrono::seconds(HEARTBEAT_INTERVAL));

            sendToMaster(msg);
        }
    }

    void  Worker::receiveTaskLoop(){
        while (running_){
            Message msg=recvTaskAssign();
            if(msg.header.type!=MessageType::TASK_ASSIGN){
                return ;
            }
            //将msg的data反序列化成TaskAssignInfo
            TaskAssignInfo taskinfo=Protocol::deserializeTaskAssignInfo(msg.data);
            //保存至task队列
            task_queue_.push(taskinfo);
            queued_task_count_++;
        }
        
    }

    void  Worker::executeTaskLoop(){
        while (running_){
        //TODO:
        //1.从task_queue_里面取出任务
        //2.执行任务
        //3.返回结果
        //思考和TaskExecutor.h,TaskExecutor.cpp之间的关系？
        
        }
    }


    void Worker::stop(){
        running_=false;
        //关闭连接，让recv退出
        if(worker_client_){
            Connection*conn=worker_client_->getConnection();
            if(conn){
                conn->disconnect();
            }
        }
        //唤醒等待任务线程
        //TODO:等以后加入 condition_variable 时再添加 task_cv_.notify_all()
        //task_cv_.notify_all();
        
        // 等待所有线程结束
        if(heartbeat_thread_.joinable()) heartbeat_thread_.join();
        if(task_recv_thread_.joinable()) task_recv_thread_.join();
        if(task_execute_thread_.joinable()) task_execute_thread_.join();
    }
}

   

