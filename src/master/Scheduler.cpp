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
            std::cout<<"no pending task!"<<std::endl;
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
        //4.构造TaskAssignInfo
        TaskAssignInfo info;
        info.task_id=task->getTaskId();
        info.payload=task->getTaskPayload();

        //5.构造TASK_ASSIGN Message
        Message msg;
        msg.header.type=MessageType::TASK_ASSIGN;
        msg.data=Protocol::serializeTaskAssignInfo(info);
        //6.发送给 Worker
        if(!worker_manager_->sendTaskToWorker(workerId,msg)){
            std::cout << "[Scheduler] "
                      << "failed to send task to Worker "
                      << workerId << std::endl;
            //分配任务失败，重新将任务添加回task_queue_
            task_manager_->pushBackTask(task);
            return false;
        }

        // 7. 发送成功后更新 Task、Master本地 queued_task_count + 1
        task->setAssignedWorker(workerId);
        task->setStatus(TaskStatus::RUNNING);
        worker_manager_->incrementWorkerQueuedTaskCount(workerId);

        //8.打印调度信息
        std::cout << "[Scheduler] Task " << task->getTaskId()
                  << " (priority=" << task->getTaskPriority() << ")"
                  << " → Worker " << workerId
                  << " (load=" << load << " → " << load + 1 << ")"
                  << std::endl;
        return true;

    }

}