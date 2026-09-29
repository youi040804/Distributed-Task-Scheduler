#include "worker/TaskDeduplicator.h"

namespace dts {

bool TaskDeduplicator::acquire(int task_id) {
    std::unique_lock<std::mutex> lock(mutex_);

    while (true) {
        // 情况 1：
        // 这个逻辑任务以前已经成功完成
        if (successful_results_.find(task_id) != successful_results_.end()) {
            return false;
        }

        // 情况 2：
        // 当前没有其他线程执行这个 task_id
        // 当前线程获得执行权
        if (executing_tasks_.find(task_id) == executing_tasks_.end()) {
            executing_tasks_.insert(task_id);
            return true;
        }

        // 情况 3：
        // 相同 task_id 正在被其他线程执行
        // 等待它成功或失败
        cv_.wait(lock);
    }
}


std::optional<std::string> TaskDeduplicator::getSuccessfulResult(int task_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = successful_results_.find(task_id);

    if (it == successful_results_.end()) {
        return std::nullopt;
    }

    return it->second;
}
void TaskDeduplicator::markSucceeded( int task_id, const std::string& result ) {
    {
        std::lock_guard<std::mutex> lock(mutex_);

        successful_results_[task_id] = result;
        executing_tasks_.erase(task_id);
    }

    cv_.notify_all();
}

void TaskDeduplicator::markFailed(int task_id) {
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // FAILED 不写 successful_results_
        executing_tasks_.erase(task_id);
    }

    cv_.notify_all();
}

}