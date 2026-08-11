/*
 * Worker.cpp
 * Worker 类的实现，包含连接 Master、注册自身等逻辑
 */
#include <arpa/inet.h>
#include <cstring>
#include <thread>
#include<iostream>//用于打印调试信息
#include<exception>
#include <stdexcept> 
#include"worker/Worker.h"
#include"common/WorkerInfo.h"


namespace dts{

    Worker::Worker(int worker_id)
        : worker_id_(worker_id),
        worker_client_(nullptr),
        executor_(std::make_unique<TaskExecutor>())//初始化executor_
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

    bool Worker::sendToMaster(const Message&msg){
        if(!worker_client_) return false;
        Connection* conn=worker_client_->getConnection();
        if(!conn) return false;
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
            
            sendToMaster(msg);// ← 先发
            std::this_thread::sleep_for(std::chrono::seconds(HEARTBEAT_INTERVAL));// ← 再睡
        }
    }
    
    // 生产者线程（receiveTaskLoop）
    void  Worker::receiveTaskLoop(){
        while (running_){
            Message msg=recvTaskAssign();
            if(msg.header.type!=MessageType::TASK_ASSIGN){
                continue; //一次异常消息不应该杀死Worker接收线程
            }
            //将msg的data反序列化成TaskAssignInfo
            TaskAssignInfo taskinfo=Protocol::deserializeTaskAssignInfo(msg.data);
            //保存至task队列
            //加锁
            {
                std::lock_guard<std::mutex>lock(task_mutex_);
                task_queue_.push(taskinfo);
            }

                queued_task_count_++; // ← atomic 操作，不需要锁
                //唤醒任务执行线程
                task_cv_.notify_one();
        }
        
    }
    
    // 消费者线程
    void  Worker::executeTaskLoop(){
        while (running_){
        TaskAssignInfo task;

        {
        //1.等待任务
        std::unique_lock<std::mutex>lock(task_mutex_);
        //停止等待的两种情况：1.Master要退出 2.队列里有任务
        task_cv_.wait(lock,[this]{
            return (!running_)||(!task_queue_.empty());
        });
        if(!running_) break;//Master停止，直接退出
        
        //2.从队列取出任务
            task=task_queue_.front();
  
            task_queue_.pop();
        }// ← 离开作用域，自动解锁
        // atomic 操作移到锁外面
        queued_task_count_--;
        running_task_count_++;

        //3.调用 TaskExecutor 执行
        TaskResultInfo result;
        try{
            result=executor_->execute(task);
        }catch(const std::exception&e){
            result={task.task_id,TaskStatus::FAILED,e.what()};
        }

        //4.更新本地计数
        running_task_count_--;

        //5.构造返回消息   
        Message msg;
        msg.header.type=MessageType::TASK_RESULT;
        msg.data=Protocol::serializeTaskResultInfo(result);
        //5.发送执行结果给Master
        //任务状态由Master来更新，Worker不负责更新任务状态
        sendToMaster(msg);
        }
    }


    void Worker::stop(){
        std::cout << "[Worker] stopping..." << std::endl;
        running_=false;
        //关闭连接，让recv退出
        if(worker_client_){
            Connection*conn=worker_client_->getConnection();
            if(conn){
                conn->disconnect();
            }
        }
        //唤醒等待任务线程
        
        task_cv_.notify_all();
        
        // 等待所有线程结束
        if(heartbeat_thread_.joinable()) heartbeat_thread_.join();

        if(task_recv_thread_.joinable()) task_recv_thread_.join();

        if(task_execute_thread_.joinable()) task_execute_thread_.join();
    }
}

   

