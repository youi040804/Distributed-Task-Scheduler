/*
*Scheduler.h
*/
#pragma once
#include<mutex>
#include"WorkerManager.h"
#include"TaskManager.h"
namespace dts{
class Scheduler{
private:
TaskManager*task_manager_;
WorkerManager*worker_manager_;

public:
Scheduler(TaskManager*tm,WorkerManager*wm);
bool schedulerOnce();


};

}