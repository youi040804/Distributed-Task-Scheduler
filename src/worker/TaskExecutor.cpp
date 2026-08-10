/*
*TaskExecutor.cpp
*/
#include<iostream>
#include<thread>
#include<chrono>
#include<exception>
#include <stdexcept> 
#include"worker/TaskExecutor.h"

// struct TaskResultInfo{
//     int task_id=0;
//     TaskStatus status=TaskStatus::FAILED;
//     std::string payload="";
// };

namespace dts{
    
TaskResultInfo TaskExecutor::execute(const TaskAssignInfo&task){
    try{
        //尝试执行任务
        if(task.payload.empty()){
            throw std::runtime_error("Empty task payload!");
        }
        
        //模拟执行耗时
        std::this_thread::sleep_for(std::chrono::seconds(1));
        //返回成功结果
        return TaskResultInfo{task.task_id,TaskStatus::DONE,"Task"+std::to_string(task.task_id)+" executed successfully"};

    }catch(const std::exception&e){
        //返回失败结果
        return TaskResultInfo{task.task_id,TaskStatus::FAILED,e.what()};
    }

}
}