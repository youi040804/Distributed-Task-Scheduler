#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

#include "client/Client.h"
#include "master/Master.h"

using namespace dts;

namespace {

constexpr int MASTER_PORT = 8084;

bool waitForTaskCreated(Master&master,int taskId,std::chrono::milliseconds timeout){
    const auto deadline=std::chrono::steady_clock::now()+timeout;

    while(std::chrono::steady_clock::now()<deadline){
        if(master.getTask(taskId).has_value()){
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}
bool isTaskStillPending(Master&master,int taskId){
    auto task=master.getTask(taskId);
    return task.has_value()
        && (*task)->getTaskStatus()==TaskStatus::PENDING
        && (*task)->getAssignedWorker()==-1
        && (*task)->getRetryCount()==0;
}
}//namespace

int main(){
    std::cout << "========================================" << std::endl;
    std::cout << "  无可用 Worker 时任务保留测试" << std::endl;
    std::cout << "========================================" << std::endl;

    Master master(MASTER_PORT);
    Client client(1);
    std::thread masterThread;
    bool passed=false;

    std::cout << "[Step 1] 启动无 Worker 的 Master..." << std::endl;
    if (!master.start()) {
        std::cerr << "❌ Master 启动失败" << std::endl;
        return 1;
    }

    masterThread = std::thread([&master]() {master.run();});

    std::cout << "[Step 2] Client 提交任务..." << std::endl;
    client.setMasterAddress("127.0.0.1", MASTER_PORT);

    if (!client.connectMaster()) {
        std::cerr << "❌ Client 连接 Master 失败" << std::endl;
    } else {
        TaskSubmitInfo taskInfo;
        taskInfo.priority=10;
        taskInfo.payload="waiting-for-worker";

        if(!client.submitTask(taskInfo)){
            std::cerr << "❌ 任务提交失败" << std::endl;
        }else if(!waitForTaskCreated(master,1,std::chrono::seconds(2))){
            std::cerr << "❌ Master 未在规定时间内创建任务" << std::endl;
        } else {
            std::cout << "[Step 3] 等待多个调度周期..." << std::endl;
            
            // Scheduler 每 100ms 执行一次；600ms 足以覆盖多次调度尝试。
            std::this_thread::sleep_for(std::chrono::milliseconds(600));

            passed=isTaskStillPending(master,1);

            if(!passed){
                auto task=master.getTask(1);
                if(task.has_value()){
                    std::cerr << "❌ 任务状态错误：任务在无 Worker 时不应被调度"<< std::endl;
                }else {
                    std::cerr << "❌ 任务丢失" << std::endl;
                }
            }
        }
    }
    std::cout << "[Step 4] 清理资源..." << std::endl;
    client.stop();
    master.stop();

    if (masterThread.joinable()) {
        masterThread.join();
    }

    assert(passed);

    std::cout << "========================================" << std::endl;
    std::cout << "  ✅ 无可用 Worker 时任务保持 PENDING" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}





