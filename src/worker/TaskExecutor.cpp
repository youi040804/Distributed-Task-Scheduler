/*
*TaskExecutor.cpp
*/
#include<iostream>
#include<thread>
#include<chrono>
#include"worker/TaskExecutor.h"

namespace dts{
    
TaskResultInfo TaskExecutor::execute(const TaskAssignInfo&task){
     // 1. 模拟执行
         std::cout << "[TaskExecutor] Executing task " << task.task_id 
              << ": " << task.payload << std::endl;
    //模拟执行耗时
    std::this_thread::sleep_for(std::chrono::seconds(1));

    //2.构造结果（假设总是成功）
    TaskResultInfo result;
    result.task_id=task.task_id;
    result.status=TaskStatus::DONE;
    result.payload="Task"+std::to_string(task.task_id)+" executed successfully";
    return result;

}
}