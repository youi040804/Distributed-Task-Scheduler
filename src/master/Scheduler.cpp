/*
*Scheduler.cpp
*/
#include"master/Scheduler.h"
#include"common/Task.h"
#include"common/Protocol.h"
#include<iostream>
namespace dts{
    Scheduler::Scheduler(TaskManager* tm, WorkerManager* wm)
        : task_manager_(tm)
        , worker_manager_(wm)
    {
        }
    bool Scheduler::schedulerOnce(){
        //1.检查是否有任务
        if(!task_manager_->hasPendingTask()){
            return false;
        }

        //2.从任务队列中取出优先级最高的任务
        auto task=task_manager_->getHighestPriorityTask();
        //任务为空直接返回
        if(!task)
        {
            return false;
        }
        //3.从worker队列取出当前负载最少的worker
        auto [workerId, load] = worker_manager_->pickLeastLoadedWorker();

        //没有可用worker，把任务放回任务队列
        if (workerId==-1) {
            std::cout << "no available worker"<<std::endl;
            task_manager_->pushBackTask(task);
            return false;

        }

        // 为本次调度生成唯一 execution_id
        uint64_t execution_id =next_execution_id_.fetch_add(1);

        // 先在 Master 中确立这次 execution
        if (!task_manager_->beginExecution(task->getTaskId(),workerId,execution_id)) {

            std::cout
                << "[Scheduler] failed to begin execution for task "
                << task->getTaskId()
                << std::endl;

            // 任务已经从 readyQueue_ 中取出，但 beginExecution 失败。
            // 需要重新放回队列，避免任务丢失。
            task_manager_->pushBackTask(task);

            return false;
        }

        // beginExecution 成功后再构造下发消息
        TaskAssignInfo info;
        info.task_id = task->getTaskId();
        info.execution_id = execution_id;
        info.payload = task->getTaskPayload();

        Message msg;
        msg.header.type = MessageType::TASK_ASSIGN;
        msg.data = Protocol::serializeTaskAssignInfo(info);

        // 最后才真正发送给 Worker
        if (!worker_manager_->sendTaskToWorker(workerId, msg)) {

            std::cout
                << "[Scheduler] failed to send task "
                << task->getTaskId()
                << ", execution "
                << execution_id
                << " to Worker "
                << workerId
                << std::endl;

            // 网络发送失败：
            // RUNNING → PENDING，并重新进入 readyQueue_
            task_manager_->rollbackExecution(task->getTaskId(),execution_id);
            return false;
        }

        //8.打印调度信息
        std::cout << "[Scheduler] Task "<< task->getTaskId()
          << " execution="<< execution_id
          << " (priority="<< task->getTaskPriority()
          << ") → Worker "<< workerId
          << " (current load="<< load<< ")"
          << std::endl;

        return true;

    }

}