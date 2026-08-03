/*
 * Task.cpp
 */

 #include"common/Task.h"

 namespace dts{

    Task::Task(int id ,int priority,const std::string& payload)
    :task_id_(id),
    task_priority_(priority),
    task_status_(TaskStatus::PENDING),
    task_payload_(payload),
    retry_count_(0),
    assigned_worker_(-1)
    {

    }
    int Task::getTaskId() const{
        return task_id_;
    }
    TaskStatus Task::getTaskStatus() const{
        return task_status_;
    }
    void Task::setStatus(TaskStatus status){
        task_status_=status;
    }

    const std::string& Task::getTaskPayload() const{
        return task_payload_;
    }
    int Task::getTaskPriority() const{
        return task_priority_;
    }
    int Task::getAssignedWorker() const{
        return assigned_worker_;
    }
    void Task::setAssignedWorker(int worker_id){
        assigned_worker_=worker_id;
    }

    int Task::getRetryCount() const{
        return retry_count_;
    }
    void Task::increaseRetryCount(){
        retry_count_++;
    }



 }