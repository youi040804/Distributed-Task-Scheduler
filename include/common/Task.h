/*
 * Task.h
 * 任务类，包含任务 ID、状态、负载、优先级、重试次数、分配 Worker
 */

#pragma once
#include<string>

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
    TaskStatus task_status_;//task_status用枚举类型，而不是字符串！
    std::string task_payload_;
    int task_priority_;
    int retry_count_;
    int assigned_worker_;   // 分配到的 Worker ID
public:
    Task(int id ,const std::string& command);//const引用string，避免发生拷贝
    int getTaskId() const;
    TaskStatus getTaskStatus() const;
    void setStatus(TaskStatus status);

    const std::string& getTaskPayload() const;
    int getTaskPriority() const;
    int getAssignedWorker() const;

    void increaseRetryCount();
    int getRetryCount() const;

};

}