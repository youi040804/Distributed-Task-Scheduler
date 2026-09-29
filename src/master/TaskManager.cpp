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
            task->setActiveExecutionId(0);
            task->setStatus(TaskStatus::PENDING);// 状态改回 PENDING
            readyQueue_.push(task);
        }
    }
    void TaskManager::pushBackTask(std::shared_ptr<Task>task){
        std::lock_guard<std::mutex> lock(task_mutex_);
        pushBackTaskUnsafe(task);  // 加锁后调用无锁版本
    }
    size_t TaskManager::recoverTasksForWorker(int workerId){
        std::lock_guard<std::mutex>lock(task_mutex_);

        size_t recoveredCount=0;
        for(const auto&[taskId,task]:tasks_){
            if(task->getTaskStatus()!=TaskStatus::RUNNING
            ||task->getAssignedWorker()!=workerId){
                continue;
            }
            
            pushBackTaskUnsafe(task);
            ++recoveredCount;
        }
        return recoveredCount;
    }

    std::optional<std::shared_ptr<Task>> TaskManager::getTask(int task_id)const{
        std::lock_guard<std::mutex>lock(task_mutex_);
        auto it =tasks_.find(task_id);
        if(it!=tasks_.end()){
            return it->second;
        }
        return std::nullopt;
    }
    std::optional<TaskSnapshot>TaskManager::getTaskSnapshot(int task_id) const {
        std::lock_guard<std::mutex> lock(task_mutex_);

        auto it = tasks_.find(task_id);

        if (it == tasks_.end()) {
            return std::nullopt;
        }

        const auto& task = it->second;

        TaskSnapshot snapshot;
        snapshot.task_id = task->getTaskId();
        snapshot.priority = task->getTaskPriority();
        snapshot.status = task->getTaskStatus();
        snapshot.payload = task->getTaskPayload();
        snapshot.result = task->getTaskResult();
        snapshot.retry_count = task->getRetryCount();
        snapshot.assigned_worker =task->getAssignedWorker();
        snapshot.active_execution_id =task->getActiveExecutionId();
        
        return snapshot;
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

    bool TaskManager::beginExecution(int task_id,int worker_id,uint64_t execution_id) {
        std::lock_guard<std::mutex> lock(task_mutex_);

        auto it = tasks_.find(task_id);

        if (it == tasks_.end()) {
            return false;
        }

        auto task = it->second;

        if (!canTransition(task->getTaskStatus(),TaskStatus::RUNNING)) {
            return false;
        }

        if (execution_id == 0) {
            return false;
        }

        task->setAssignedWorker(worker_id);
        task->setActiveExecutionId(execution_id);
        task->setStatus(TaskStatus::RUNNING);

        return true;
    }
    bool TaskManager::rollbackExecution(int task_id,uint64_t execution_id) {
        std::lock_guard<std::mutex> lock(task_mutex_);

        auto it = tasks_.find(task_id);
        if (it == tasks_.end()) {
            return false;
        }

        auto task = it->second;

        // 只允许回滚当前这一次 execution
        if (task->getTaskStatus() != TaskStatus::RUNNING ||
            task->getActiveExecutionId() != execution_id) {
            return false;
        }

        pushBackTaskUnsafe(task);

        return true;
    }

    ProcessTaskResult TaskManager::processTaskResult(int task_id,uint64_t execution_id,const std::string& result_data,const TaskStatus& status){
        std::lock_guard<std::mutex> lock(task_mutex_);

        auto it = tasks_.find(task_id);

        if(it == tasks_.end()){
            return {ProcessResultCode::NOT_FOUND, -1};
        }

        auto task = it->second;
        TaskStatus oldStatus = task->getTaskStatus();

        if (task->getActiveExecutionId() != execution_id) {
            return {ProcessResultCode::STALE_EXECUTION,-1};
        }
        
        if(!canTransition(oldStatus, status)){
            return {ProcessResultCode::INVALID_TRANSITION, -1};
        }

        int workerId = task->getAssignedWorker();

        if(status == TaskStatus::FAILED){

            task->increaseRetryCount();

            if(task->getRetryCount() <= MAX_TASK_RETRY){
                // 还有重试次数，重新进入待调度队列
                pushBackTaskUnsafe(task);


                return {ProcessResultCode::RETRY,workerId};

            }else{
                // 重试次数耗尽，最终失败
                task->setStatus(TaskStatus::FAILED);
                task->setAssignedWorker(-1);
                task->setActiveExecutionId(0);

                return {ProcessResultCode::FINAL_FAILED,workerId};
            }
        }

        // 正常成功完成
        task->setTaskResult(result_data);
        task->setStatus(status);
        task->setAssignedWorker(-1);
        task->setActiveExecutionId(0);
       
        return {ProcessResultCode::SUCCESS, workerId};
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

