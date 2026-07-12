#pragma once

#include<iostream>
#include<string>

enum class TaskStatus
{
PENDING,
RUNNING,
FAILED,
DONE
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
    std::string getTaskPayload();
    int getTaskPriority();
    void setStatus(TaskStatus status);
};

