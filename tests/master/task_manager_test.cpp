/*
*test_manager_test.cpp
*/
#include<cassert>
#include<iostream>
#include"master/TaskManager.h"
#include"utils/Config.h"
using namespace dts;
void testPriorityScheduling(){
    std::cout<<"====Test 1: 按优先级去除任务==="<<std::endl;
    TaskManager manager;
    manager.addTask(Task(1,1,"low"));
    manager.addTask(Task(2,10,"high"));
    manager.addTask(Task(3,5,"medium"));

    assert(manager.getHighestPriorityTask()->getTaskId()==2);
    assert(manager.getHighestPriorityTask()->getTaskId()==3);
    assert(manager.getHighestPriorityTask()->getTaskId()==1);

    std::cout << "✅ 高优先级任务优先出队" << std::endl;
}

void testTaskStateMachine(){
    std::cout<<"=== Test 2: 任务状态机 ==="<<std::endl;
    TaskManager manager;
    manager.addTask(Task(1,1,"task"));
    //合法状态转换
    assert(manager.updateTaskStatus(1,TaskStatus::RUNNING));
    assert(manager.updateTaskStatus(1,TaskStatus::DONE));

    auto task=manager.getTask(1);
    assert(task.has_value());
    assert((*task)->getTaskStatus()==TaskStatus::DONE);

    //DONE后不能再次进入RUNNING
    assert(!manager.updateTaskStatus(1,TaskStatus::RUNNING));

    //不存在的任务不能更新状态
    assert(!manager.updateTaskStatus(999,TaskStatus::RUNNING));
     std::cout << "✅ 合法状态转换通过，非法状态转换被拒绝" << std::endl;
}

void testTaskSuccessResult(){
    std::cout << "=== Test 3: 任务成功结果处理 ===" << std::endl;
    TaskManager manager;
    manager.addTask(Task(1, 1, "task"));

    assert(manager.updateTaskStatus(1,TaskStatus::RUNNING));

    auto task=manager.getTask(1);
    assert(task.has_value());
    (*task)->setAssignedWorker(7);

    auto workerId=manager.processTaskResult(1,"success",TaskStatus::DONE);
    assert(workerId.has_value());
    assert(*workerId==7);

    task=manager.getTask(1);
    assert((*task)->getTaskStatus()==TaskStatus::DONE);
    assert((*task)->getAssignedWorker()==-1);
    
    std::cout << "✅ 任务成功后状态更新为 DONE，并清除 Worker 绑定" << std::endl;
}

void testTaskRetryLimit() {
    std::cout << "=== Test 4: 任务失败重试次数 ===" << std::endl;

    TaskManager manager;
    manager.addTask(Task(1, 1, "task"));

    // 失败后应重新调度 MAX_TASK_RETRY 次
    for(int retry=1;retry<=MAX_TASK_RETRY;++retry){
        auto task=manager.getHighestPriorityTask();
        assert(task!=nullptr);

        assert(manager.updateTaskStatus(1,TaskStatus::RUNNING));
        auto result=manager.processTaskResult(1,"Failed",TaskStatus::FAILED);

        assert(!result.has_value());

        auto stored=manager.getTask(1);
        assert(stored.has_value());
        assert((*stored)->getRetryCount()==retry);
        assert((*stored)->getTaskStatus()==TaskStatus::PENDING);
    }
    // 第 MAX_TASK_RETRY + 1 次失败，任务最终 FAILED
    auto task=manager.getHighestPriorityTask();
    assert(task!=nullptr);
    assert(manager.updateTaskStatus(1,TaskStatus::RUNNING));;

    auto result =manager.processTaskResult(1, "failed", TaskStatus::FAILED);

    assert(!result.has_value());

    auto stored=manager.getTask(1);
    assert(stored.has_value());
    assert((*stored)->getTaskStatus()==TaskStatus::FAILED);
    assert((*stored)->getAssignedWorker()==-1);
    assert(!manager.hasPendingTask());

    std::cout << "✅ 任务失败后最多重试 "<< MAX_TASK_RETRY<< " 次，超出后最终失败" << std::endl;
}
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  TaskManager 单元测试" << std::endl;
    std::cout << "========================================" << std::endl;

    testPriorityScheduling();
    testTaskStateMachine();
    testTaskSuccessResult();
    testTaskRetryLimit();

    std::cout << "========================================" << std::endl;
    std::cout << "  ✅ 所有测试通过！" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}