#include <cassert>     
#include <iostream>    
#include <thread>      
#include <vector>      

#include "worker/Worker.h"

using namespace dts;

int main() {
    std::cout << "=== Worker dedup test ===" << std::endl;

    Worker worker(1, 4);

    constexpr int task_id = 42;
    constexpr int thread_count = 4;

    std::vector<TaskResultInfo> results(thread_count);
    std::vector<std::thread> threads;

    threads.reserve(thread_count);

    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back([&, i]() {
            TaskAssignInfo task;
            task.task_id = task_id;
            task.execution_id = 1001 + i;
            task.payload = "same-logical-task";

            results[i] = worker.executeTask(task);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    const std::string expected = "Task42 executed successfully";

    for (int i = 0; i < thread_count; ++i) {
        assert(results[i].task_id == task_id);

        // 每个返回都必须绑定自己的 execution_id
        assert( results[i].execution_id == static_cast<uint64_t>(1001 + i) );

        assert(results[i].status == TaskStatus::DONE);

        // 真正执行者和 cache follower得到相同的逻辑执行结果
        assert(results[i].payload == expected);
    }

    // 再提交一次相同逻辑任务,此时应该直接命中成功缓存
    TaskAssignInfo cached_task;
    cached_task.task_id = task_id;
    cached_task.execution_id = 2001;
    cached_task.payload = "same-logical-task";

    TaskResultInfo cached_result = worker.executeTask(cached_task);
    assert(cached_result.task_id == task_id);
    assert(cached_result.execution_id == 2001);
    assert(cached_result.status == TaskStatus::DONE);
    assert(cached_result.payload == expected);

    std::cout << "Worker dedup result reuse passed" << std::endl;

    return 0;
}