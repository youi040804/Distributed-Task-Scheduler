#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

#include "master/Master.h"
#include "utils/Config.h"
#include "worker/Worker.h"

using namespace dts;

namespace {

constexpr int MASTER_PORT = 8083;
constexpr int WORKER_ID = 1;
constexpr int WORKER_PORT = 9003;

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
bool waitForHeartbeatAdvance(Master&master,int workerId,
                             std::chrono::system_clock::time_point previousTime,
                             std::chrono::milliseconds timeout){

    const auto deadline=std::chrono::steady_clock::now()+timeout;

    while(std::chrono::steady_clock::now()<deadline){
        auto worker=master.getWorkerInfo(workerId);

        if(worker.has_value()
           &&worker->getLastHeartbeatTime()>previousTime
           &&worker->isAlive()){
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
}

bool waitForWorkerDead(Master&master,int workerId,std::chrono::milliseconds timeout){
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now()<deadline){
        auto worker=master.getWorkerInfo(workerId);
        if(worker.has_value()&&!worker->isAlive()){
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

    }
    return false;
    
}
}//namespace

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  心跳与超时集成测试" << std::endl;
    std::cout << "========================================" << std::endl;

    Master master(MASTER_PORT);
    Worker worker(WORKER_ID);
    std::thread masterThread;

    bool workerStarted = false;
    bool heartbeatUpdated = false;
    bool workerTimedOut = false;
    
    std::cout << "[Step 1] 启动 Master..." << std::endl;
    if (!master.start()) {
        std::cerr << "❌ Master 启动失败" << std::endl;
        return 1;
    }

    masterThread = std::thread([&master]() {
        master.run();
    });

    std::cout << "[Step 2] 启动 Worker 并注册..." << std::endl;
    workerStarted = worker.start(
        "127.0.0.1", MASTER_PORT, "127.0.0.1", WORKER_PORT);

    if (!workerStarted) {
        std::cerr << "❌ Worker 启动失败" << std::endl;
    } else if (!waitForWorker(master, WORKER_ID,
                              std::chrono::seconds(2))) {
        std::cerr << "❌ Master 未在规定时间内记录 Worker" << std::endl;
    } else {

        auto workerInfo=master.getWorkerInfo(WORKER_ID);
        const auto firstHeartbeatTime=workerInfo->getLastHeartbeatTime();

        std::cout << "[Step 3] 等待下一次真实心跳..." << std::endl;
        heartbeatUpdated = waitForHeartbeatAdvance(
        master, WORKER_ID, firstHeartbeatTime,
        std::chrono::seconds(HEARTBEAT_INTERVAL + 2));
        
    if (!heartbeatUpdated) {
        std::cerr << "❌ 未观察到 Worker 的下一次心跳" << std::endl;
    } else {
        std::cout << "✅ 心跳已刷新，Worker 保持存活" << std::endl;
        std::cout << "[Step 4] 停止 Worker，等待 Master 判定超时..."<< std::endl;

        worker.stop();
        workerStarted=false;
                    // 超时判定最多可能经过一个检测周期才触发。
        workerTimedOut = waitForWorkerDead(
            master, WORKER_ID,
            std::chrono::seconds(HEARTBEAT_TIMEOUT + HEARTBEAT_CHECK_INTERVAL + 3));
        if (!workerTimedOut) {
            std::cerr << "❌ Worker 未在规定时间内被标记为死亡"<< std::endl;
        }
    }
}

    std::cout << "[Step 5] 清理资源..." << std::endl;
    if (workerStarted) {
        worker.stop();
    }
    master.stop();

    if (masterThread.joinable()) {
        masterThread.join();
    }

    assert(heartbeatUpdated);
    assert(workerTimedOut);
    std::cout << "========================================" << std::endl;
    std::cout << "  ✅ 心跳刷新与超时检测均通过" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}