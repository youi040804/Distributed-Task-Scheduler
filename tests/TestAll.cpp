#include <iostream>
#include <cassert>
#include <memory>
#include <optional>

#include "master/TaskManager.h"
#include "master/WorkerManager.h"
#include "master/Scheduler.h"

#include "client/Client.h"

#include "common/Task.h"
#include "common/Message.h"
#include "common/Protocol.h"

#include "network/Connection.h"

using namespace dts;

/*
 FakeConnection
 模拟 TCP 发送成功
 Scheduler / Client 只关心: sendMessage()
 不需要真实 socket
*/
class FakeConnection : public Connection
{
public:
    Message lastMessage;
    bool sendShouldFail = false;

    FakeConnection()
        : Connection(-1, sockaddr_in{})
    {
    }

    bool sendMessage(const Message& msg) override
    {
        if (sendShouldFail) {
            return false;
        }
        lastMessage = msg;
        std::cout << "[FakeConnection] send message type=" 
                  << static_cast<int>(msg.header.type) 
                  << std::endl;
        return true;
    }
};

/*
 测试 Protocol TaskSubmitInfo
*/
void testTaskSubmitProtocol()
{
    std::cout << "\n===== Test TaskSubmit Protocol =====" << std::endl;

    TaskSubmitInfo info;
    info.priority = 10;
    info.payload = "hello task";

    std::string data = Protocol::serializeTaskSubmitInfo(info);

    auto result = Protocol::deserializeTaskSubmitInfo(data);

    assert(result.priority == 10);
    assert(result.payload == "hello task");

    std::cout << "TaskSubmit Protocol PASS" << std::endl;
}

/*
 测试 Protocol TaskAssignInfo
*/
void testTaskAssignProtocol()
{
    std::cout << "\n===== Test TaskAssign Protocol =====" << std::endl;

    TaskAssignInfo info;
    info.task_id = 100;
    info.payload = "execute this task";

    std::string data = Protocol::serializeTaskAssignInfo(info);

    auto result = Protocol::deserializeTaskAssignInfo(data);

    assert(result.task_id == 100);
    assert(result.payload == "execute this task");

    std::cout << "TaskAssign Protocol PASS" << std::endl;
}

/*
 测试 TaskManager
*/
void testTaskManager()
{
    std::cout << "\n===== Test TaskManager =====" << std::endl;

    TaskManager manager;

    Task task1(1, 10, "high priority task");
    Task task2(2, 5, "low priority task");

    manager.addTask(std::move(task1));
    manager.addTask(std::move(task2));

    auto task = manager.getHighestPriorityTask();
    assert(task != nullptr);
    assert(task->getTaskId() == 1);

    std::cout << "Priority queue PASS" << std::endl;

    bool result = manager.updateTaskStatus(1, TaskStatus::RUNNING);
    assert(result);

    result = manager.updateTaskStatus(1, TaskStatus::DONE);
    assert(result);

    std::cout << "Task status PASS" << std::endl;
}

/*
 测试失败重试机制
*/
void testTaskRetry()
{
    std::cout << "\n===== Test Task Retry =====" << std::endl;

    TaskManager manager;

    Task task(100, 10, "retry task");
    manager.addTask(std::move(task));

    auto taskPtr = manager.getHighestPriorityTask();
    assert(taskPtr != nullptr);

    taskPtr->setStatus(TaskStatus::RUNNING);
    taskPtr->setAssignedWorker(1);

    auto workerId = manager.processTaskResult(100, "failed", TaskStatus::FAILED);
    assert(!workerId.has_value());

    auto retryTask = manager.getTask(100);
    assert(retryTask.has_value());
    assert(retryTask.value()->getRetryCount() == 1);
    assert(retryTask.value()->getTaskStatus() == TaskStatus::PENDING);
    assert(retryTask.value()->getAssignedWorker() == -1);

    std::cout << "Retry mechanism PASS" << std::endl;
}

/*
 测试 WorkerManager
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

    assert(manager.updateWorkerHeartbeat(1));
    assert(manager.updateWorkerLoad(1, 3, 2));

    auto info = manager.getWorkerInfo(1);
    assert(info.has_value());
    assert(info->getWorkerLoad() == 5);

    std::cout << "Worker load PASS" << std::endl;
}

/*
 测试 Scheduler
*/
void testScheduler()
{
    std::cout << "\n===== Test Scheduler =====" << std::endl;

    TaskManager taskManager;
    WorkerManager workerManager;

    auto conn = std::make_shared<FakeConnection>();

    WorkerInfo worker(1, "127.0.0.1", 9001);
    workerManager.addWorker(std::move(worker), conn);
    workerManager.updateWorkerLoad(1, 0, 0);

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
 测试 Client 提交任务（单向通知模型）
 注意：验证的是 Protocol 序列化/反序列化，不是真正的 Client 集成测试
 真正的 Client 集成测试需要 Master 启动和 TCP 连接
*/
void testClientSubmitTask()
{
    std::cout << "\n===== Test Client Submit Task (Protocol Only) =====" << std::endl;

    // 注意：这个测试验证的是 TaskSubmitInfo 的序列化/反序列化
    // 以及 Message 类型标记的正确性
    // 不是真正的 Client 集成测试
    
    TaskSubmitInfo info;
    info.priority = 5;
    info.payload = "client test task";
    
    Message msg;
    // 修正：使用正确的消息类型 SUBMIT_TASK
    msg.header.type = MessageType::SUBMIT_TASK;
    msg.data = Protocol::serializeTaskSubmitInfo(info);
    
    auto conn = std::make_shared<FakeConnection>();
    bool result = conn->sendMessage(msg);
    assert(result);
    
    // 验证消息类型正确
    assert(conn->lastMessage.header.type == MessageType::SUBMIT_TASK);
    
    // 验证数据可以正确反序列化
    auto deserialized = Protocol::deserializeTaskSubmitInfo(conn->lastMessage.data);
    assert(deserialized.priority == 5);
    assert(deserialized.payload == "client test task");
    
    std::cout << "Client submit task protocol PASS" << std::endl;
    std::cout << "WARNING: This is protocol test only, not full Client integration" << std::endl;
}

/*
 测试多个 Worker 的场景
 注意：只验证能找到可用 Worker，不假设具体 ID
 因为 unordered_map 的遍历顺序是不确定的
*/
void testMultipleWorkers()
{
    std::cout << "\n===== Test Multiple Workers =====" << std::endl;

    WorkerManager manager;

    // 注册 3 个 Worker
    for (int i = 1; i <= 3; i++) {
        auto conn = std::make_shared<FakeConnection>();
        WorkerInfo worker(i, "127.0.0.1", 9000 + i);
        manager.addWorker(std::move(worker), conn);
        bool result = manager.updateWorkerLoad(i, 0, 0);
        assert(result);
    }

    // 验证所有 Worker 都已注册
    for (int i = 1; i <= 3; i++) {
        assert(manager.hasWorker(i));
    }

    // 获取最空闲的 Worker
    // 注意：不假设具体 ID，因为 unordered_map 遍历顺序不确定
    auto [workerId, load] = manager.pickLeastLoadedWorker();
    assert(workerId != -1);  // 只要能找到可用 Worker
    assert(load == 0);      // 负载应该为 0

    std::cout << "Picked worker: " << workerId << ", load: " << load << std::endl;

    // 更新 Worker1 的负载
    manager.updateWorkerLoad(1, 5, 3);

    // 再次获取最空闲的 Worker
    auto [workerId2, load2] = manager.pickLeastLoadedWorker();
    assert(workerId2 != -1);
    assert(load2 == 0);  // 至少有一个 Worker 负载为 0
    
    std::cout << "Picked worker: " << workerId2 << ", load: " << load2 << std::endl;

    // 验证选择的不是 Worker1（负载为 8）
    assert(workerId2 != 1);

    std::cout << "Multiple workers load balancing PASS" << std::endl;
}

/*
 测试任务状态转换
 注意：测试的是 TaskManager 的状态管理，不是完整的任务生命周期
 完整的生命周期需要经过 Worker 执行和 TASK_RESULT 处理
*/
void testTaskLifecycle()
{
    std::cout << "\n===== Test Task State Management =====" << std::endl;

    TaskManager taskManager;
    WorkerManager workerManager;

    // 1. 注册 Worker
    auto conn = std::make_shared<FakeConnection>();
    WorkerInfo worker(1, "127.0.0.1", 9000);
    workerManager.addWorker(std::move(worker), conn);
    workerManager.updateWorkerLoad(1, 0, 0);

    // 2. 提交任务
    Task task(1, 10, "lifecycle test task");
    taskManager.addTask(std::move(task));
    assert(taskManager.getTask(1).has_value());
    assert(taskManager.getTask(1).value()->getTaskStatus() == TaskStatus::PENDING);
    std::cout << "Step 1: Task submitted (PENDING) ✓" << std::endl;

    // 3. 调度任务（通过 Scheduler）
    Scheduler scheduler(&taskManager, &workerManager);
    bool result = scheduler.schedulerOnce();
    assert(result);
    
    auto scheduledTask = taskManager.getTask(1);
    assert(scheduledTask.has_value());
    assert(scheduledTask.value()->getTaskStatus() == TaskStatus::RUNNING);
    assert(scheduledTask.value()->getAssignedWorker() == 1);
    std::cout << "Step 2: Task scheduled (RUNNING) ✓" << std::endl;

    // 4. 任务完成（直接更新状态）
    // 注意：真正的完成应该通过 Worker 的 TASK_RESULT 消息触发
    result = taskManager.updateTaskStatus(1, TaskStatus::DONE);
    assert(result);
    
    auto completedTask = taskManager.getTask(1);
    assert(completedTask.has_value());
    assert(completedTask.value()->getTaskStatus() == TaskStatus::DONE);
    std::cout << "Step 3: Task completed (DONE) ✓" << std::endl;

    std::cout << "Task state management PASS" << std::endl;
    std::cout << "WARNING: Full lifecycle requires Worker execution and TASK_RESULT" << std::endl;
}

/*
 测试 Worker 超时检测
 注意：测试的是 markWorkerDead 功能，不是真正的心跳超时检测
 真正的心跳超时检测需要时间模拟
*/
void testWorkerTimeout()
{
    std::cout << "\n===== Test Worker Dead Mark =====" << std::endl;

    WorkerManager manager;

    // 注册 Worker
    auto conn = std::make_shared<FakeConnection>();
    WorkerInfo worker(1, "127.0.0.1", 9000);
    manager.addWorker(std::move(worker), conn);

    // 初始状态：Worker 是活的
    assert(manager.hasWorker(1));
    auto info = manager.getWorkerInfo(1);
    assert(info.has_value());
    assert(info->isAlive());

    // 获取超时 Worker 列表（应该为空）
    auto timeoutList = manager.getTimeoutWorker();
    assert(timeoutList.empty());
    std::cout << "Step 1: No timeout worker ✓" << std::endl;

    // 手动标记 Worker 为 Dead（模拟超时检测后的处理）
    // 注意：真正的超时检测需要 getTimeoutWorker() 基于时间判断
    bool result = manager.markWorkerDead(1);
    assert(result);

    // 验证 Worker 已被标记为 Dead
    auto deadInfo = manager.getWorkerInfo(1);
    assert(deadInfo.has_value());
    assert(!deadInfo->isAlive());
    std::cout << "Step 2: Worker marked as dead ✓" << std::endl;

    // Dead Worker 不应该被 pickLeastLoadedWorker 选中
    auto [workerId, load] = manager.pickLeastLoadedWorker();
    assert(workerId == -1);
    assert(load == 0);
    std::cout << "Step 3: Dead worker not selected ✓" << std::endl;

    std::cout << "Worker dead mark PASS" << std::endl;
    std::cout << "WARNING: Full timeout detection requires time simulation" << std::endl;
}

/*
 测试 sendTaskToWorker
 修正：使用正确的 TaskAssignInfo 序列化
*/
void testSendTaskToWorker()
{
    std::cout << "\n===== Test Send Task To Worker =====" << std::endl;

    WorkerManager manager;

    // 1. 注册 Worker
    auto conn = std::make_shared<FakeConnection>();
    WorkerInfo worker(1, "127.0.0.1", 9000);
    manager.addWorker(std::move(worker), conn);
    manager.updateWorkerLoad(1, 0, 0);

    // 2. 构造任务消息
    Task task(1, 10, "test task");
    Message msg;
    msg.header.type = MessageType::TASK_ASSIGN;
    
    // 修正：使用 TaskAssignInfo 而不是 TaskSubmitInfo
    TaskAssignInfo assignInfo;
    assignInfo.task_id = task.getTaskId();
    assignInfo.payload = task.getTaskPayload();
    msg.data = Protocol::serializeTaskAssignInfo(assignInfo);

    // 3. 发送任务到 Worker
    bool result = manager.sendTaskToWorker(1, msg);
    assert(result);

    // 验证消息已发送且类型正确
    auto fakeConn = std::dynamic_pointer_cast<FakeConnection>(conn);
    assert(fakeConn);
    assert(fakeConn->lastMessage.header.type == MessageType::TASK_ASSIGN);
    
    // 验证数据可以被 Worker 正确反序列化
    auto deserialized = Protocol::deserializeTaskAssignInfo(fakeConn->lastMessage.data);
    assert(deserialized.task_id == 1);
    assert(deserialized.payload == "test task");
    std::cout << "Task sent to worker successfully ✓" << std::endl;

    // 4. 测试发送到不存在的 Worker
    result = manager.sendTaskToWorker(999, msg);
    assert(!result);
    std::cout << "Send to non-existent worker fails correctly ✓" << std::endl;

    // 5. 测试发送到 Dead Worker
    manager.markWorkerDead(1);
    result = manager.sendTaskToWorker(1, msg);
    assert(!result);
    std::cout << "Send to dead worker fails correctly ✓" << std::endl;

    std::cout << "Send task to worker PASS" << std::endl;
}

int main()
{
    std::cout << "========== DTS TEST ==========" << std::endl;

    // 基础功能测试
    testTaskSubmitProtocol();
    testTaskAssignProtocol();  // 新增：测试 TaskAssign 协议
    testTaskManager();
    testTaskRetry();
    testWorkerManager();
    testScheduler();

    // 协议和功能测试
    testClientSubmitTask();
    testMultipleWorkers();
    testTaskLifecycle();
    testWorkerTimeout();
    testSendTaskToWorker();

    std::cout << "\n========== ALL TEST PASS ==========" << std::endl;
    std::cout << "\n=== IMPORTANT NOTES ===" << std::endl;
    std::cout << "1. testClientSubmitTask: Protocol test only, not full Client integration" << std::endl;
    std::cout << "2. testTaskLifecycle: State management test, full lifecycle requires Worker" << std::endl;
    std::cout << "3. testWorkerTimeout: Dead mark test, full timeout requires time simulation" << std::endl;
    std::cout << "4. testMultipleWorkers: Does not assume specific Worker ID due to unordered_map" << std::endl;

    return 0;
}