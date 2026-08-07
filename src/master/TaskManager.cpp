/*
*TaskManager.cpp
*/
#include<iostream>
#include"master/TaskManager.h"
namespace dts{

    void TaskManager::addTask(Task task){
        std::lock_guard<std::mutex>lock(task_mutex_);
        auto taskPtr=std::make_shared<Task>(std::move(task));

        tasks_.emplace(taskPtr->getTaskId(),taskPtr);
        readyQueue_.push(taskPtr);
    }

    void TaskManager::pushBackTask(std::shared_ptr<Task>task){
        std::lock_guard<std::mutex>lock(task_mutex_);
        if(task){
            task->setStatus(TaskStatus::PENDING);// 状态改回 PENDING
            readyQueue_.push(task);
        }
    }

    std::optional<std::shared_ptr<Task>> TaskManager::getTask(int task_id)const{
        std::lock_guard<std::mutex>lock(task_mutex_);
        auto it =tasks_.find(task_id);
        if(it!=tasks_.end()){
            return it->second;
        }
        return std::nullopt;
    }

    bool TaskManager::updateTaskStatus(int task_id,TaskStatus status){
        std::lock_guard<std::mutex>lock(task_mutex_);
        auto it =tasks_.find(task_id);
        if(it!=tasks_.end()){
            it->second->setStatus(status);
            return true;
        }
        return false;
    }
    bool TaskManager::removeTask(int task_id){
        //逻辑错误❌️暂时不实现，以后再设计
        // std::lock_guard<std::mutex>lock(task_mutex_);
        // if(tasks_.erase(task_id))
             return true;
        // return false;
    }

    bool TaskManager::hasPendingTask(){
        std::lock_guard<std::mutex>lock(task_mutex_);
        if(readyQueue_.empty()){
            return false;
        }else return true;
    }

    //找到priority最大的task，每次拿最高优先级任务
    std::shared_ptr<Task> TaskManager::getHighestPriorityTask(){
        std::lock_guard<std::mutex>lock(task_mutex_);

        if(readyQueue_.empty())
        return nullptr;
        auto taskPtr=readyQueue_.top();
        readyQueue_.pop();
        return taskPtr;
    }

}

