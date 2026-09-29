/*
 * task_manager_test.cpp
 */

#include <cassert>   // assert
#include <cstdint>   // uint64_t
#include <iostream>  // cout

#include "master/TaskManager.h"
#include "utils/Config.h"

using namespace dts;


// ============================================================
// Test 1: 优先级调度
// ============================================================
void testPriorityScheduling() {
    std::cout << "=== Test 1: 按优先级取出任务 ===" << std::endl;

    TaskManager manager;

    manager.addTask(Task(1, 1, "low"));
    manager.addTask(Task(2, 10, "high"));
    manager.addTask(Task(3, 5, "medium"));

    assert(manager.getHighestPriorityTask()->getTaskId() == 2);
    assert(manager.getHighestPriorityTask()->getTaskId() == 3);
    assert(manager.getHighestPriorityTask()->getTaskId() == 1);

    std::cout << " 高优先级任务优先出队" << std::endl;
}


// ============================================================
// Test 2: 基础任务状态机
// ============================================================
void testTaskStateMachine() {
    std::cout << "=== Test 2: 任务状态机 ===" << std::endl;

    TaskManager manager;
    manager.addTask(Task(1, 1, "task"));

    // 合法状态转换：
    // PENDING -> RUNNING -> DONE
    assert(manager.updateTaskStatus( 1, TaskStatus::RUNNING ));

    assert(manager.updateTaskStatus( 1, TaskStatus::DONE ));
    auto task = manager.getTask(1);

    assert(task.has_value());
    assert( (*task)->getTaskStatus() == TaskStatus::DONE );

    // DONE 后不能再次进入 RUNNING
    assert(!manager.updateTaskStatus( 1, TaskStatus::RUNNING ));

    // 不存在的任务不能更新状态
    assert(!manager.updateTaskStatus( 999, TaskStatus::RUNNING ));
    std::cout << " 合法状态转换通过，非法状态转换被拒绝" << std::endl;
}


// ============================================================
// Test 3: 任务成功结果处理
// ============================================================
void testTaskSuccessResult() {
    std::cout << "=== Test 3: 任务成功结果处理 ===" << std::endl;

    TaskManager manager;

    manager.addTask(Task(1, 1, "task"));

    // 模拟 Scheduler 开始一次真正的 execution：
    //
    // Task 1
    // -> Worker 7
    // -> execution_id = 1001
    assert(manager.beginExecution( 1, 7, 1001 ));

    // 验证 beginExecution 后的状态
    auto snapshot = manager.getTaskSnapshot(1);

    assert(snapshot.has_value());
    assert(snapshot->status == TaskStatus::RUNNING);
    assert(snapshot->assigned_worker == 7);
    assert(snapshot->active_execution_id == 1001);

    // 模拟 Worker 7 返回 execution 1001 的成功结果
    auto result = manager.processTaskResult( 1, 1001, "success", TaskStatus::DONE );

    assert( result.code == ProcessResultCode::SUCCESS );

    assert(result.worker_id == 7);

    // 成功以后：
    //
    // status = DONE
    // assigned_worker = -1
    // active_execution_id = 0
    // result = "success"
    snapshot = manager.getTaskSnapshot(1);

    assert(snapshot.has_value());
    assert(snapshot->status == TaskStatus::DONE);
    assert(snapshot->assigned_worker == -1);
    assert(snapshot->active_execution_id == 0);
    assert(snapshot->result == "success");

    std::cout << "任务成功后更新为 DONE，并清除 Worker 和 execution 绑定" << std::endl;
}


// ============================================================
// Test 4: 任务失败重试次数
// ============================================================
void testTaskRetryLimit() {
    std::cout << "=== Test 4: 任务失败重试次数 ===" << std::endl;

    TaskManager manager;

    manager.addTask(Task(1, 1, "task"));

    // 前 MAX_TASK_RETRY 次失败：
    //
    // RUNNING -> FAILED result
    //         -> RETRY
    //         -> PENDING
    //
    // 每一次重试都必须使用新的 execution_id
    for (int retry = 1; retry <= MAX_TASK_RETRY; ++retry) {

        // 模拟 Scheduler 从 readyQueue_ 中取出任务
        auto task = manager.getHighestPriorityTask();

        assert(task != nullptr);

        const uint64_t execution_id = static_cast<uint64_t>(retry);
        // 模拟重新调度到 Worker 7
        assert(manager.beginExecution( 1, 7,execution_id));

        // 验证当前 execution 已经建立
        auto running_snapshot =manager.getTaskSnapshot(1);

        assert(running_snapshot.has_value());
        assert(running_snapshot->status ==TaskStatus::RUNNING);

        assert(running_snapshot->assigned_worker ==7);

        assert(running_snapshot->active_execution_id ==execution_id);

        // 当前 execution 执行失败
        auto result =manager.processTaskResult(1,execution_id,"Failed",TaskStatus::FAILED);

        assert(result.code ==ProcessResultCode::RETRY);

        assert(result.worker_id == 7);

        auto stored =manager.getTaskSnapshot(1);

        assert(stored.has_value());

        // 重试次数正确增加
        assert(stored->retry_count ==retry);

        // 任务重新进入 PENDING
        assert(stored->status ==TaskStatus::PENDING);

        // 上一次 execution 已经失效
        assert(stored->assigned_worker ==-1);
        assert(stored->active_execution_id ==0);
    }

    // ========================================================
    // 第 MAX_TASK_RETRY + 1 次失败：
    // 不再重新入队，而是进入最终 FAILED
    // ========================================================

    auto task =manager.getHighestPriorityTask();

    assert(task != nullptr);

    const uint64_t final_execution_id =static_cast<uint64_t>(MAX_TASK_RETRY + 1);

    assert(manager.beginExecution(1,7,final_execution_id));

    auto result =manager.processTaskResult(1,final_execution_id,"failed",TaskStatus::FAILED);

    assert(result.code ==ProcessResultCode::FINAL_FAILED);

    assert(result.worker_id == 7);

    auto stored =manager.getTaskSnapshot(1);

    assert(stored.has_value());

    assert(stored->status ==TaskStatus::FAILED);

    // 最终失败后不再绑定 Worker
    assert(stored->assigned_worker ==-1);

    // 最终失败后不存在 active execution
    assert(stored->active_execution_id ==0);

    // 已经最终失败，因此不能再存在于待调度队列
    assert(!manager.hasPendingTask());

    std::cout<< "任务失败后最多重试 "<< MAX_TASK_RETRY<< " 次，超出后最终失败"<< std::endl;
}


// ============================================================
// main
// ============================================================
int main() {
    std::cout<< "========================================"<< std::endl;

    std::cout<< "  TaskManager 单元测试"<< std::endl;

    std::cout<< "========================================"<< std::endl;

    testPriorityScheduling();
    testTaskStateMachine();
    testTaskSuccessResult();
    testTaskRetryLimit();

    std::cout<< "========================================"<< std::endl;

    std::cout<< "   所有测试通过！"<< std::endl;

    std::cout<< "========================================"<< std::endl;

    return 0;
}