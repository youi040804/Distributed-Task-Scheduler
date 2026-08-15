#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

#include "client/Client.h"
#include "master/Master.h"
#include "worker/Worker.h"

using namespace dts;

namespace {
constexpr int MASTER_PORT=8086;

constexpr int WORKER_1_ID = 1;
constexpr int WORKER_1_PORT = 9006;

constexpr int WORKER_2_ID = 2;
constexpr int WORKER_2_PORT = 9007;

bool waitForWorker(Master& master,
                   int workerId,
                   std::chrono::milliseconds timeout) {
    const auto deadline =
        std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
        if (master.getWorkerInfo(workerId).has_value()) {
            return true;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(50));
    }

    return false;
}

bool waitForTaskAssignedTo(Master& master,
                           int taskId,
                           int workerId,
                           std::chrono::milliseconds timeout) {
    const auto deadline =std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
        auto task=master.getTask(taskId);

        if(task.has_value()
           &&(*task)->getTaskStatus()==TaskStatus::RUNNING
           &&(*task)->getAssignedWorker()==workerId){
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}

bool waitForTaskDone(Master& master,
                     int taskId,
                     std::chrono::milliseconds timeout) {
    const auto deadline =std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
        auto task=master.getTask(taskId);
        if (task.has_value()
            && (*task)->getTaskStatus() == TaskStatus::DONE
            && (*task)->getAssignedWorker() == -1) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}
}//namespace

int main(){
    std::cout << "========================================"<< std::endl;
    std::cout << "  Worker 超时任务恢复集成测试"<< std::endl;
    std::cout << "========================================"<< std::endl;

    Master master(MASTER_PORT);
    Worker worker1(WORKER_1_ID);
    Worker worker2(WORKER_2_ID);
    Client client(1);
    
    std::thread masterThread;

    bool worker1Started = false;
    bool worker2Started = false;
    bool taskAssignedToWorker1 = false;
    bool taskReassignedToWorker2 = false;
    bool taskDone = false;

    std::cout << "[Step 1] 启动 Master..." << std::endl;

    if (!master.start()) {
        std::cerr << "❌ Master 启动失败" << std::endl;
        return 1;
    }
    masterThread = std::thread([&master]() {
        master.run();
    });

    std::cout << "[Step 2] 启动两个 Worker..." << std::endl;

    worker1Started = worker1.start("127.0.0.1", MASTER_PORT,"127.0.0.1", WORKER_1_PORT);
    worker2Started = worker2.start("127.0.0.1", MASTER_PORT,"127.0.0.1", WORKER_2_PORT);


    if (!worker1Started || !worker2Started) {
        std::cerr << "❌ Worker 启动失败" << std::endl;
    } else if (!waitForWorker(master, WORKER_1_ID,std::chrono::seconds(2))
                ||!waitForWorker(master, WORKER_2_ID,std::chrono::seconds(2))){
        std::cerr << "❌ Master 未记录全部 Worker"<< std::endl;
    } else {
        std::cout << "✅ 两个 Worker 均注册成功"<< std::endl;
        std::cout << "[Step 3] 提交任务，等待 Worker 1 接收..."<< std::endl;

        client.setMasterAddress("127.0.0.1", MASTER_PORT);

        if (!client.connectMaster()) {
            std::cerr << "❌ Client 连接 Master 失败"<< std::endl;
        } else {
            TaskSubmitInfo taskInfo;
            taskInfo.priority = 10;
            taskInfo.payload = "recovery-task";

            if (!client.submitTask(taskInfo)) {
                std::cerr << "❌ 任务提交失败" << std::endl;
            } else {
                taskAssignedToWorker1 = waitForTaskAssignedTo(master, 1, WORKER_1_ID,std::chrono::seconds(3));

                if (!taskAssignedToWorker1) {
                    std::cerr << "❌ 任务未分配给 Worker 1"<< std::endl;
                } else {
                    std::cout << "✅ 任务已分配给 Worker 1"<< std::endl;
                    std::cout<< "[Step 4] 停止 Worker 1，模拟执行期间失联..."<< std::endl;

                    worker1.stop();
                    worker1Started = false;

                    std::cout<< "[Step 5] 等待超时恢复并重新调度给 Worker 2..."<< std::endl;

                    taskReassignedToWorker2 =waitForTaskAssignedTo(master, 1, WORKER_2_ID,std::chrono::seconds(20));

                    if (!taskReassignedToWorker2) {
                        std::cerr<< "❌ 任务未重新调度给 Worker 2"<< std::endl;
                    } else {
                        std::cout<< "✅ 超时任务已重新调度给 Worker 2"<< std::endl;

                        taskDone = waitForTaskDone(master, 1,std::chrono::seconds(3));

                        if (!taskDone) {
                            std::cerr<< "❌ Worker 2 未完成恢复后的任务"<< std::endl;
                        }
                    }
                }
            }
        }
    }

    std::cout << "[Step 6] 清理资源..." << std::endl;

    client.stop();

    if (worker1Started) {
        worker1.stop();
    }

    if (worker2Started) {
        worker2.stop();
    }

    master.stop();

    if (masterThread.joinable()) {
        masterThread.join();
    }

    assert(taskAssignedToWorker1);
    assert(taskReassignedToWorker2);
    assert(taskDone);

    std::cout << "========================================"<< std::endl;
    std::cout<< "  ✅ Worker 超时后任务恢复并重新执行成功"<< std::endl;
    std::cout << "========================================"<< std::endl;

    return 0;

}