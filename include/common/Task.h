/*
 * Task.h
 * 任务类，包含任务 ID、状态、负载、优先级、重试次数、分配 Worker
 */

#pragma once
#include<string>
#include <cstdint> // uint64_t
#include<memory>
namespace dts{
enum class TaskStatus
{
    PENDING=0,
    RUNNING=1,
    FAILED=2,
    DONE=3
};
class Task{
private:
    int task_id_;
    int task_priority_;
    TaskStatus task_status_;//task_status用枚举类型，而不是字符串！
    std::string task_payload_;
    std::string task_result_;//任务最终执行结果
    int retry_count_;
    int assigned_worker_;   // 分配到的 Worker ID
    uint64_t active_execution_id_;

public:
    Task(int id ,int priority,const std::string& payload);//const引用string，避免发生拷贝
    int getTaskId() const;
    TaskStatus getTaskStatus() const;
    void setStatus(TaskStatus status);

    const std::string& getTaskPayload() const;
    int getTaskPriority() const;
    int getAssignedWorker() const;
    void setAssignedWorker(int worker_id);
    
    const std::string& getTaskResult() const;
    void setTaskResult(const std::string& result);
    
    void increaseRetryCount();
    int getRetryCount() const;
    
    uint64_t getActiveExecutionId() const;
    void setActiveExecutionId(uint64_t execution_id);
};

}