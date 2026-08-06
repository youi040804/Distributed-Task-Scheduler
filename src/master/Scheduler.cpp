/*
*Scheduler.cpp
*/
#include"master/Scheduler.h"
#include"common/Task.h"
#include<iostream>
namespace dts{
Scheduler::Scheduler(TaskManager*tm,WorkerManager*wm)
                    :task_manager_(tm),worker_manager_(wm){

}
bool Scheduler::schedulerOnce(){
    //1.检查是否有任务
    if(!task_manager_->hasPendingTask()){
        std::cout<<"no pending task!"<<std::endl;
        return false;
    }

    //2.从任务队列中取出优先级最高的任务
    auto HighestPriorityTask=task_manager_->getHighestPriorityTask();
    //任务为空直接返回
    if(!HighestPriorityTask)
    {
        return false;
    }
    //3.从worker队列取出当前运行任务最少的worker
    auto [workerId, load] = worker_manager_->pickLeastLoadedWorker();

    //没有可用worker，把任务放回任务队列
    if (workerId==-1) {
        std::cout << "no available worker"<<std::endl;
        task_manager_->pushBackTask(HighestPriorityTask);
        return false;

    }
    //4.分派任务给worker
    HighestPriorityTask->setAssignedWorker(workerId);
    worker_manager_->increaseWorkerTaskCount(workerId);

    //5.打印调度信息
    std::cout << "[Scheduler] Task " << HighestPriorityTask->getTaskId() 
            << " (priority=" << HighestPriorityTask->getTaskPriority() << ")"
            << " → Worker " << workerId 
            << " (load=" << load << ")" << std::endl;
    return true;

}


}