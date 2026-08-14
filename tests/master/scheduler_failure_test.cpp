#include <cassert>
#include <iostream>
#include <memory>

#include "master/Scheduler.h"
#include "master/TaskManager.h"
#include "master/WorkerManager.h"
#include "network/Connection.h"

using namespace dts;

namespace {
class FailingConnection:public Connection{
public:
    FailingConnection():Connection(-1,sockaddr_in{}){}
    bool sendMessage(const Message&)override{
        return false;
    }
};

void testTaskIsReturnedWhenSendFails(){
    std::cout << "=== Test: 发送任务失败后重新入队 ===" << std::endl;

    TaskManager taskManager;
    WorkerManager workerManager;
    Scheduler scheduler(&taskManager,&workerManager);

    auto failingConnection=std::make_shared<FailingConnection>();

    workerManager.addWorker(WorkerInfo(1,"127.0.0.1",9001),failingConnection);
    taskManager.addTask(Task(1,10,"task-payload"));
    // Worker 可用，但模拟 TASK_ASSIGN 的 TCP 发送失败。
    assert(!scheduler.schedulerOnce());

    auto task=taskManager.getTask(1);
    assert(task.has_value());
    assert((*task)->getTaskStatus()==TaskStatus::PENDING);
    assert((*task)->getAssignedWorker()==-1);
    assert((*task)->getRetryCount()==0);

    // 再次取出，证明任务确实被重新放回待调度队列。
    auto retryTask = taskManager.getHighestPriorityTask();
    assert(retryTask!=nullptr);
    assert(retryTask->getTaskId()==1);
    std::cout << "✅ 发送失败后任务保持 PENDING 并重新入队"<< std::endl;
}
}//namespace

int main(){
    std::cout << "========================================" << std::endl;
    std::cout << "  Scheduler 发送失败回退单元测试" << std::endl;
    std::cout << "========================================" << std::endl;

    testTaskIsReturnedWhenSendFails();

    std::cout << "========================================" << std::endl;
    std::cout << "  ✅ 所有测试通过！" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}