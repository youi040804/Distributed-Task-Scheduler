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
#include"common/Task.h"
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
    bool updateTaskStatus(int task_id,TaskStatus status);
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
};


}