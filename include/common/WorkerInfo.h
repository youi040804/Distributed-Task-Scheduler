#pragma once

#include<iostream>
#include<string>
#include<chrono>
struct WorkerInfo{

    int worker_id_;
    size_t running_task_count_;
    std::chrono::system_clock::time_point  last_heartbeat_time_;//只记录最后一次Heartbeat
    bool alive_;

};
