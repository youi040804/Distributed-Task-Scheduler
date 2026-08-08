/*
*TaskExecutor.h
*/
#pragma once
#include"common/Protocol.h"
//Task相关的参考
// struct TaskResultInfo{
//     int task_id=0;
//     TaskStatus status=TaskStatus::FAILED;
//     std::string payload="";
// };
// struct TaskAssignInfo{
//     int task_id=0;
//     std::string payload="";
// };
// class Task{
// private:
//     int task_id_;
//     int task_priority_;
//     TaskStatus task_status_;//task_status用枚举类型，而不是字符串！
//     std::string task_payload_;
//     int retry_count_;
//     int assigned_worker_;   // 分配到的 Worker ID

// public:
//     Task(int id ,int priority,const std::string& payload);

namespace dts{
class TaskExecutor{
private:

public:
TaskResultInfo execute(const TaskAssignInfo&task);

};
}