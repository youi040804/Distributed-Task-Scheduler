#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

#include "master/Master.h"
#include "worker/Worker.h"

using namespace dts;

namespace {

constexpr int MASTER_PORT = 8085;
constexpr int WORKER_ID = 1;
constexpr int WORKER_PORT = 9005;

constexpr auto MAX_STOP_TIME=std::chrono::seconds(1);
}//namespacek


int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  优雅停止集成测试" << std::endl;
    std::cout << "========================================" << std::endl;

    Master master(MASTER_PORT);
    assert(master.start());

    std::thread masterThread([&master](){
        master.run();
    });

    Worker worker(WORKER_ID);
    assert(worker.start("127.0.0.1",MASTER_PORT,"127.0.0.1",WORKER_PORT));

    // 确保 Worker 心跳线程已进入等待状态
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::cout << "[Step 1] 停止 Worker..." << std::endl;
    const auto workerStopBegin = std::chrono::steady_clock::now();

    worker.stop();

    const auto workerStopCost=std::chrono::steady_clock::now()-workerStopBegin;
    std::cout << "   Worker 停止耗时："
              << std::chrono::duration_cast<std::chrono::milliseconds>(workerStopCost).count()
              << " ms" << std::endl;

        std::cout << "[Step 2] 停止 Master..." << std::endl;
    const auto masterStopBegin = std::chrono::steady_clock::now();

    master.stop();

    const auto masterStopCost =std::chrono::steady_clock::now() - masterStopBegin;

    if (masterThread.joinable()) {
        masterThread.join();
    }
    std::cout << "   Master 停止耗时："
            << std::chrono::duration_cast<std::chrono::milliseconds>(masterStopCost).count()
            << " ms" << std::endl;
    assert(workerStopCost<MAX_STOP_TIME);
    assert(masterStopCost<MAX_STOP_TIME);

    std::cout << "========================================" << std::endl;
    std::cout << "  ✅ Worker 与 Master 均可快速停止"<< std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}

