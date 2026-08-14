/*
*task_e2e_test.cpp
*/

#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

#include "client/Client.h"
#include "master/Master.h"
#include "worker/Worker.h"

using namespace dts;

namespace {
constexpr int MASTER_PORT=8081;
constexpr int WORKER_ID=1;
constexpr int WORKER_PORT=9001;

bool waitForWorker(Master&master,int workerId,std::chrono::milliseconds timeout){
    const auto deadline=std::chrono::steady_clock::now()+timeout;

    while(std::chrono::steady_clock::now()<deadline){
        if(master.getWorkerInfo(workerId).has_value()){
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}

bool waitForTaskDone(Master&master,int taskId,std::chrono::milliseconds timeout){
    const auto deadline=std::chrono::steady_clock::now()+timeout;

    while(std::chrono::steady_clock::now()<deadline){
        auto task=master.getTask(taskId);
        if(task.has_value()&&(*task)->getTaskStatus()==TaskStatus::DONE){
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}
}//namespace

int main(){

    std::cout << "========================================" << std::endl;
    std::cout << "  正常任务闭环集成测试" << std::endl;
    std::cout << "========================================" << std::endl;

    Master master(MASTER_PORT);
    Worker worker(WORKER_ID);
    Client client(1);
    std::thread masterThread;
    bool passed=false;
    std::cout << "[Step 1] 启动 Master..." << std::endl;

    if(!master.start()){
        std::cerr<<"❌️Master 启动失败"<<std::endl;
        return 1;
    }

    masterThread=std::thread([&master](){master.run();});

    std::cout << "[Step 2] 启动 Worker 并注册..." << std::endl;
    if(!worker.start("127.0.0.1",MASTER_PORT,"127.0.0.1",WORKER_PORT)){
        std::cerr << "❌ Worker 启动失败" << std::endl;
    }else if(!waitForWorker(master,WORKER_ID,std::chrono::seconds(2))){
         std::cerr << "❌ Master 未在规定时间内记录 Worker" << std::endl;
    }else {
        std::cout << "✅ Worker 注册成功" << std::endl;

        std::cout << "[Step 3] Client 提交正常任务..." << std::endl;
        client.setMasterAddress("127.0.0.1",MASTER_PORT);
        if(!client.connectMaster()){
           std::cerr << "❌ Client 连接 Master 失败" << std::endl;
        } else {
            TaskSubmitInfo taskInfo;
            taskInfo.priority=10;
            taskInfo.payload="normal-task";
            if(!client.submitTask(taskInfo)){
                std::cerr << "❌ 任务提交失败" << std::endl;
            } else {
                std::cout << "[Step 4] 等待任务执行完成..." << std::endl;
                passed=waitForTaskDone(master,1,std::chrono::seconds(5));

                if(!passed){
                    std::cerr << "❌ 任务未在 5 秒内变为 DONE" << std::endl;
                }
            }
        }
    }
    std::cout << "[Step 5] 清理资源..." << std::endl;
    client.stop();
    worker.stop();
    master.stop();

    if(masterThread.joinable()){
        masterThread.join();
    }

    assert(passed);
    
    std::cout << "========================================" << std::endl;
    std::cout << "  ✅ Client → Master → Worker → DONE 闭环通过"<< std::endl;
    std::cout << "========================================" << std::endl;

    return 0;

}