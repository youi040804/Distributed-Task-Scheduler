/*
 * TaskManager.h
 */
#pragma once
#include<unordered_map>
#include<mutex>
#include<optional>
#include<queue>
#include<vector>
#include<memory>
#include<set>
#include<utility>
#include"common/Task.h"
#include"common/Protocol.h"

namespace dts{
class TaskManager{

public:
    //自定义比较器-priority大的优先级高
    struct TaskPriorityComparator{
        bool operator()(const std::shared_ptr<Task>&a,
                        const std::shared_ptr<Task>&b)const{
            return a->getTaskPriority()< b->getTaskPriority();
        }
    };

private:
    std::unordered_map<int,std::shared_ptr<Task>> tasks_;
    std::priority_queue<std::shared_ptr<Task>,
                        std::vector<std::shared_ptr<Task>>,
                        TaskPriorityComparator>readyQueue_;//根据t<ask的priority进行排序
    mutable std::mutex task_mutex_;

public:
    void addTask(Task task);
    void pushBackTask(std::shared_ptr<Task>task);//新增“把任务放回队列”的方法
    
    std::optional<std::shared_ptr<Task>> getTask(int task_id)const;
    //通用状态修改器—— 用于"分配任务"等只需要改状态的场景
    bool updateTaskStatus(int task_id,TaskStatus newStatus);
    // 新增 —— 专门用于"任务完成"场景
    std::optional<int> processTaskResult(int task_id, const std::string& result_data,const TaskStatus&status);

    bool removeTask(int task_id);
    bool hasPendingTask();

    template<typename Func>
    void forEachTask(Func&&func)const{
        std::lock_guard<std::mutex>lock(task_mutex_);
        for(const auto&[id,ptr]:tasks_){
            func(ptr);
        }

    }

    std::shared_ptr<Task> getHighestPriorityTask();//找到priority最大的task，每次拿最高优先级任务
    std::set<std::pair<TaskStatus,TaskStatus>>validTransitions={
        {TaskStatus::PENDING, TaskStatus::RUNNING},  // 正常调度
        {TaskStatus::RUNNING, TaskStatus::DONE},     // 执行成功
        {TaskStatus::RUNNING, TaskStatus::FAILED},   // 执行失败
        {TaskStatus::FAILED, TaskStatus::PENDING},   // 重试
    };
    bool canTransition(TaskStatus oldStatus,TaskStatus newStatus){
        return validTransitions.count({oldStatus,newStatus})>0;
    }
};


}