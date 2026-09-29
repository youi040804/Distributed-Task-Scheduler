/*
*Scheduler.h
*/
#pragma once
#include<mutex>
#include <atomic>  // std::atomic
#include <cstdint> // uint64_t
#include"WorkerManager.h"
#include"TaskManager.h"
namespace dts{
class Scheduler{
private:
    TaskManager*task_manager_;
    WorkerManager*worker_manager_;
    std::atomic<uint64_t> next_execution_id_{1};

public:
    Scheduler(TaskManager*tm,WorkerManager*wm);
    bool schedulerOnce();

};

}