#include <iostream>
#include <cassert>
#include <memory>
#include "master/TaskManager.h"
#include "master/WorkerManager.h"
#include "master/Scheduler.h"
#include "common/Task.h"
#include "common/Message.h"
#include "network/Connection.h"

using namespace dts;

/*
    FakeConnection
    模拟 TCP 发送成功
    Scheduler 只关心：sendMessage()
    不需要真正建立 socket
*/
class FakeConnection : public Connection
{

public:

    FakeConnection()
        : Connection(-1, sockaddr_in{})
    {
    }


    bool sendMessage(Message& msg) override
    {
        std::cout
            << "[FakeConnection] send message type="
            << static_cast<int>(msg.header.type)
            << std::endl;

        return true;
    }

};
/*
==========================
测试 TaskManager
==========================
*/
void testTaskManager()
{
    std::cout << "\n===== Test TaskManager =====" << std::endl;
    
    TaskManager manager;
    Task task1(1, 10, "high priority task");
    Task task2(2, 5, "low priority task");
    
    manager.addTask(std::move(task1));
    manager.addTask(std::move(task2));
    
    // 优先级队列测试：priority 10 应该先出来
    auto task = manager.getHighestPriorityTask();
    assert(task != nullptr);
    assert(task->getTaskId() == 1);
    std::cout << "Priority queue PASS" << std::endl;

    // 状态流转测试：PENDING → RUNNING → DONE
    bool result = manager.updateTaskStatus(1, TaskStatus::RUNNING);
    assert(result);
    
    result = manager.updateTaskStatus(1, TaskStatus::DONE);
    assert(result);
    std::cout << "Task status PASS" << std::endl;
}


/*
==========================
测试 WorkerManager
==========================
*/
void testWorkerManager()
{
    std::cout << "\n===== Test WorkerManager =====" << std::endl;
    
    WorkerManager manager;
    auto conn = std::make_shared<FakeConnection>();
    
    WorkerInfo worker(1, "127.0.0.1", 9000);
    manager.addWorker(std::move(worker), conn);
    
    assert(manager.hasWorker(1));
    std::cout << "Worker register PASS" << std::endl;

    // 心跳测试
    assert(manager.updateWorkerHeartbeat(1));
    std::cout << "Heartbeat PASS" << std::endl;

    // Load 同步测试
    assert(manager.updateWorkerLoad(1, 3, 2));
    
    auto info = manager.getWorkerInfo(1);
    assert(info.has_value());
    assert(info->getWorkerLoad() == 5);
    std::cout << "Worker load PASS" << std::endl;
}


/*
==========================
测试 Scheduler
==========================
*/
void testScheduler()
{
    std::cout << "\n===== Test Scheduler =====" << std::endl;

    TaskManager taskManager;
    WorkerManager workerManager;

    auto conn = std::make_shared<FakeConnection>();

    WorkerInfo worker(1, "127.0.0.1", 9001);
    workerManager.addWorker(std::move(worker), conn);

    Task task(1, 100, "execute task");
    taskManager.addTask(std::move(task));

    Scheduler scheduler(&taskManager, &workerManager);

    bool result = scheduler.schedulerOnce();
    assert(result);

    auto finishedTask = taskManager.getTask(1);
    assert(finishedTask.has_value());
    assert(finishedTask.value()->getAssignedWorker() == 1);
    assert(finishedTask.value()->getTaskStatus() == TaskStatus::RUNNING);
    
    std::cout << "Scheduler PASS" << std::endl;
}


/*
==========================
main
==========================
*/
int main()
{
    std::cout << "========== DTS TEST ==========" << std::endl;
    
    testTaskManager();
    testWorkerManager();
    testScheduler();
    
    std::cout << "\n========== ALL TEST PASS ==========" << std::endl;
    return 0;
}