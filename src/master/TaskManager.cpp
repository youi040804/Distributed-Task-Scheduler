/*
*TaskManager.cpp
*/
#include<iostream>
#include"master/TaskManager.h"
#include"common/Message.h"
#include"utils/Config.h"
namespace dts{

    void TaskManager::addTask(Task task){
        std::lock_guard<std::mutex>lock(task_mutex_);
        auto taskPtr=std::make_shared<Task>(std::move(task));

        tasks_.emplace(taskPtr->getTaskId(),taskPtr);
        readyQueue_.push(taskPtr);
    }

    void TaskManager::pushBackTaskUnsafe(std::shared_ptr<Task>task){
        // 不加锁！调用者必须已经持有 task_mutex_
        if(task){
            task->setAssignedWorker(-1); //在入队时自动清除assigned_worker
            task->setStatus(TaskStatus::PENDING);// 状态改回 PENDING
            readyQueue_.push(task);
        }
    }
    void TaskManager::pushBackTask(std::shared_ptr<Task>task){
        std::lock_guard<std::mutex> lock(task_mutex_);
        pushBackTaskUnsafe(task);  // 加锁后调用无锁版本
    }

    std::optional<std::shared_ptr<Task>> TaskManager::getTask(int task_id)const{
        std::lock_guard<std::mutex>lock(task_mutex_);
        auto it =tasks_.find(task_id);
        if(it!=tasks_.end()){
            return it->second;
        }
        return std::nullopt;
    }
    bool TaskManager::updateTaskStatus(int task_id,TaskStatus newStatus){
        std::lock_guard<std::mutex>lock(task_mutex_);
        auto it =tasks_.find(task_id);
        if(it==tasks_.end()){
            return false;
        }
        TaskStatus oldStatus=it->second->getTaskStatus();
        if(!canTransition(oldStatus,newStatus)){
            std::cout<<"非法状态转移!"<<std::endl;
            return false;
        }
        it->second->setStatus(newStatus);
        return true;
    }

     std::optional<int> TaskManager::processTaskResult(int task_id, const std::string& result_data,const TaskStatus&status){
        std::lock_guard<std::mutex>lock(task_mutex_);
        auto it =tasks_.find(task_id);
        if(it==tasks_.end()){
            return std::nullopt;
        }
        auto task=it->second;
        TaskStatus oldStatus=task->getTaskStatus();
        if(!canTransition(oldStatus,status)){
            return std::nullopt;
        }
        
        if(status==TaskStatus::FAILED){
            task->increaseRetryCount();

            if(task->getRetryCount()<MAX_TASK_RETRY){
                // 还有重试次数，重新调度
                pushBackTaskUnsafe(task);
                return std::nullopt; //重试时返回空，表示没有 worker
            }else{
                // 重试次数用完，最终失败
                task->setStatus(TaskStatus::FAILED);
                task->setAssignedWorker(-1);  //清除 assigned_worker
                return std::nullopt;  //最终失败也返回空
            }
        }else{
            task->setStatus(status);
        }
        int workerId=task->getAssignedWorker();
        task->setAssignedWorker(-1);//成功后清除，准备下一个任务
        return workerId;
        // result_data是任务的"输出"——Worker 执行完任务的最终产物
        // processTaskResult 把它存到 Task 对象里
        // 后续有需要的话可以使用，目前暂时不实现
    }
    
    bool TaskManager::removeTask(int task_id){
       //暂时返回false，后期再实现具体逻辑

        return false;
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

