#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

#include "master/Scheduler.h"
#include "master/TaskManager.h"
#include "master/WorkerManager.h"
#include "common/Protocol.h"
#include "network/TCPServer.h"
#include "network/TCPClient.h"

using namespace dts;

int main()
{
    constexpr int TEST_PORT = 19001;
    constexpr int WORKER_ID = 1;

    std::cout << "========== Task Assignment Test ==========" << std::endl;

    // ------------------------------------------------------------
    // 1. 启动一个假的 Worker 端 TCP Server
    // ------------------------------------------------------------
    TCPServer worker_server(TEST_PORT);

    assert(worker_server.start());
    std::cout << "[Test] Worker server started." << std::endl;

    // ------------------------------------------------------------
    // 2. 创建 WorkerManager / TaskManager / Scheduler
    // ------------------------------------------------------------
    WorkerManager worker_manager;
    TaskManager task_manager;

    Scheduler scheduler(&task_manager, &worker_manager);

    // ------------------------------------------------------------
    // 3. 模拟 Worker 连接 Master
    // ------------------------------------------------------------
    TCPClient worker_client("127.0.0.1", TEST_PORT);

    assert(worker_client.connect());
    std::cout << "[Test] Worker connected to server." << std::endl;

    auto connection = worker_server.acceptConnection();

    assert(connection != nullptr);
    std::cout << "[Test] Server accepted Worker connection." << std::endl;

    // ------------------------------------------------------------
    // 4. 把 Worker 加入 WorkerManager
    // ------------------------------------------------------------
    WorkerInfo worker(
        WORKER_ID,
        "127.0.0.1",
        TEST_PORT
    );

    worker_manager.addWorker(
        std::move(worker),
        connection
    );

    assert(worker_manager.hasWorker(WORKER_ID));

    std::cout << "[Test] Worker registered in WorkerManager."
              << std::endl;

    // ------------------------------------------------------------
    // 5. 调度前验证 Worker 负载为 0
    // ------------------------------------------------------------
    auto workerInfoBefore = worker_manager.getWorkerInfo(WORKER_ID);
    assert(workerInfoBefore.has_value());

    size_t loadBefore = workerInfoBefore.value().getWorkerLoad();
    size_t queuedBefore = workerInfoBefore.value().getQueuedTaskCount();
    size_t runningBefore = workerInfoBefore.value().getRunningTaskCount();

    assert(loadBefore == 0);
    assert(queuedBefore == 0);
    assert(runningBefore == 0);
    assert(loadBefore == queuedBefore + runningBefore);

    std::cout << "[Test] Before scheduling: "
              << "load=" << loadBefore
              << " (queued=" << queuedBefore
              << ", running=" << runningBefore << ")"
              << std::endl;

    // ------------------------------------------------------------
    // 6. 向 TaskManager 添加一个任务
    // ------------------------------------------------------------
    Task task(
        1,                  // task id
        10,                 // priority
        "hello worker"      // payload
    );

    task_manager.addTask(std::move(task));

    assert(task_manager.hasPendingTask());

    std::cout << "[Test] Task added to TaskManager."
              << std::endl;

    // ------------------------------------------------------------
    // 7. Scheduler 执行一次调度
    // ------------------------------------------------------------
    bool result = scheduler.schedulerOnce();

    assert(result);

    std::cout << "[Test] Scheduler dispatched task successfully."
              << std::endl;

    // ------------------------------------------------------------
    // 8. 验证 Master 本地负载视图（核心验证点）
    // ------------------------------------------------------------
    auto workerInfoAfter = worker_manager.getWorkerInfo(WORKER_ID);
    assert(workerInfoAfter.has_value());

    size_t loadAfter = workerInfoAfter.value().getWorkerLoad();
    size_t queuedAfter = workerInfoAfter.value().getQueuedTaskCount();
    size_t runningAfter = workerInfoAfter.value().getRunningTaskCount();

    // 验证负载组成
    assert(queuedAfter == 1);           // 任务已进入 Worker 队列
    assert(runningAfter == 0);          // Worker 还没开始执行
    assert(loadAfter == 1);             // queued + running = 1
    assert(loadAfter == queuedAfter + runningAfter);  // 验证 load 定义

    std::cout << "[Test] After scheduling: "
              << "load=" << loadAfter
              << " (queued=" << queuedAfter
              << ", running=" << runningAfter << ")"
              << std::endl;

    // ------------------------------------------------------------
    // 9. Worker 端接收 TASK_ASSIGN
    // ------------------------------------------------------------
    Message msg = worker_client.getConnection()->receiveMessage();

    assert(msg.header.type == MessageType::TASK_ASSIGN);

    std::cout << "[Test] Worker received TASK_ASSIGN."
              << std::endl;

    // ------------------------------------------------------------
    // 10. 解析 TASK_ASSIGN
    // ------------------------------------------------------------
    TaskAssignInfo assignInfo =
        Protocol::deserializeTaskAssignInfo(msg.data);

    assert(assignInfo.task_id == 1);
    assert(assignInfo.payload == "hello worker");

    std::cout << "[Test] Task ID: "
              << assignInfo.task_id << std::endl;

    std::cout << "[Test] Task payload: "
              << assignInfo.payload << std::endl;

    // ------------------------------------------------------------
    // 11. 验证 Master 本地 Task 状态
    // ------------------------------------------------------------
    auto storedTask = task_manager.getTask(1);

    assert(storedTask.has_value());
    assert(storedTask.value()->getTaskStatus()
           == TaskStatus::RUNNING);

    assert(storedTask.value()->getAssignedWorker()
           == WORKER_ID);

    std::cout << "[Test] Task status = RUNNING." << std::endl;

    std::cout << "[Test] Assigned Worker = "
              << storedTask.value()->getAssignedWorker()
              << std::endl;

    // ------------------------------------------------------------
    // 12. 清理连接
    // ------------------------------------------------------------
    worker_client.getConnection()->disconnect();
    worker_server.stop();

    std::cout << "=========================================="
              << std::endl;
    std::cout << "Task Assignment Test PASSED!"
              << std::endl;

    return 0;
}