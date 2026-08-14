/*
*task_retry_e2e_test.cpp
*/
#include<cassert>
#include <chrono>
#include <iostream>
#include <thread>

#include "client/Client.h"
#include "master/Master.h"
#include "utils/Config.h"
#include "worker/Worker.h"

using namespace dts;

namespace {

constexpr int MASTER_PORT = 8082;
constexpr int WORKER_ID = 1;
constexpr int WORKER_PORT = 9002;

bool waitForWorker(Master& master, int workerId,
                   std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
        if (master.getWorkerInfo(workerId).has_value()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    return false;
}
bool waitForFinalFailure(Master&master,int taskId,std::chrono::milliseconds timeout){
    const auto deadline=std::chrono::steady_clock::now()+timeout;

    while(std::chrono::steady_clock::now()<deadline){
        auto task=master.getTask(taskId);

        if(task.has_value()
           &&(*task)->getTaskStatus()==TaskStatus::FAILED
           &&(*task)->getRetryCount()==MAX_TASK_RETRY+1
           &&(*task)->getAssignedWorker()==-1){
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}
}//namespace

int main(){
    std::cout << "========================================" << std::endl;
    std::cout << "  任务失败重试闭环集成测试" << std::endl;
    std::cout << "========================================" << std::endl;

    Master master(MASTER_PORT);
    Worker worker(WORKER_ID);
    Client client(1);
    std::thread masterThread;
    bool passed = false;
    
    std::cout << "[Step 1] 启动 Master..." << std::endl;
    if (!master.start()) {
        std::cerr << "❌ Master 启动失败" << std::endl;
        return 1;
    }
    masterThread=std::thread([&master]{return master.run();});

    std::cout << "[Step 2] 启动 Worker 并注册..." << std::endl;
    if (!worker.start("127.0.0.1", MASTER_PORT,
                      "127.0.0.1", WORKER_PORT)) {
        std::cerr << "❌ Worker 启动失败" << std::endl;
    } else if (!waitForWorker(master, WORKER_ID,
                              std::chrono::seconds(2))) {
        std::cerr << "❌ Master 未在规定时间内记录 Worker" << std::endl;
    } else {
        std::cout << "✅ Worker 注册成功" << std::endl;

        std::cout << "[Step 3] Client 提交空 payload 任务..." << std::endl;
        client.setMasterAddress("127.0.0.1", MASTER_PORT);

        if (!client.connectMaster()) {
            std::cerr << "❌ Client 连接 Master 失败" << std::endl;
        } else {
            TaskSubmitInfo taskInfo;
            taskInfo.priority = 10;
            taskInfo.payload = "";

            if (!client.submitTask(taskInfo)) {
                std::cerr << "❌ 任务提交失败" << std::endl;
            } else {
                std::cout << "[Step 4] 等待 3 次重试后最终失败..."
                          << std::endl;

                passed = waitForFinalFailure(master, 1,
                                             std::chrono::seconds(5));

                if (!passed) {
                    auto task = master.getTask(1);

                    if (task.has_value()) {
                        std::cerr << "❌ 任务未满足最终失败条件，当前 retry_count = "
                                  << (*task)->getRetryCount() << std::endl;
                    } else {
                        std::cerr << "❌ Master 中未找到任务" << std::endl;
                    }
                }
            }
        }
    }

    std::cout << "[Step 5] 清理资源..." << std::endl;
    client.stop();
    worker.stop();
    master.stop();

    if (masterThread.joinable()) {
        masterThread.join();
    }

    assert(passed);

    std::cout << "========================================" << std::endl;
    std::cout << "  ✅ 任务重试 3 次后最终 FAILED" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;

}