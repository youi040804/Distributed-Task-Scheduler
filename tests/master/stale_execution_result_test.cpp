#include <cassert>
#include <iostream>

#include "master/TaskManager.h"

using namespace dts;

int main() {
    std::cout << "=== Stale Execution Result Test ===" << std::endl;

    TaskManager manager;

    // 创建逻辑任务
    manager.addTask(Task(1, 5, "test-task"));

    // 模拟第一次调度：
    // Task 1 → Worker 1, execution 1001
    assert(manager.beginExecution( 1, 1, 1001 ));
    auto snapshot = manager.getTaskSnapshot(1);

    assert(snapshot.has_value());
    assert(snapshot->status == TaskStatus::RUNNING);
    assert(snapshot->assigned_worker == 1);
    assert(snapshot->active_execution_id == 1001);

    // 模拟第一次 execution 失效并重新入队
    assert(manager.rollbackExecution( 1, 1001 ));

    snapshot = manager.getTaskSnapshot(1);

    assert(snapshot.has_value());
    assert(snapshot->status == TaskStatus::PENDING);
    assert(snapshot->assigned_worker == -1);
    assert(snapshot->active_execution_id == 0);

    // 模拟重新调度：
    // Task 1 → Worker 2, execution 1002
    assert(manager.beginExecution( 1, 2, 1002 ));

    // 此时旧 Worker 1 才返回 execution 1001 的结果
    auto stale_result = manager.processTaskResult( 1, 1001, "old-result", TaskStatus::DONE );

    assert( stale_result.code == ProcessResultCode::STALE_EXECUTION );

    // 关键验证：
    // stale result 不能污染当前 execution 1002
    snapshot = manager.getTaskSnapshot(1);

    assert(snapshot.has_value());
    assert(snapshot->status == TaskStatus::RUNNING);
    assert(snapshot->assigned_worker == 2);
    assert(snapshot->active_execution_id == 1002);
    assert(snapshot->result.empty());

    // 当前 execution 1002 返回结果
    auto current_result = manager.processTaskResult( 1, 1002, "new-result", TaskStatus::DONE );

    assert( current_result.code == ProcessResultCode::SUCCESS );

    // 最终任务正常完成
    snapshot = manager.getTaskSnapshot(1);

    assert(snapshot.has_value());
    assert(snapshot->status == TaskStatus::DONE);
    assert(snapshot->assigned_worker == -1);
    assert(snapshot->active_execution_id == 0);
    assert(snapshot->result == "new-result");

    std::cout << "✅ stale execution result 被正确忽略" << std::endl;

    return 0;
}