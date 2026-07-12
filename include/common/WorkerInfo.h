#pragma once

#include<string>
#include<chrono>
namespace dts{
class WorkerInfo{
private:
    int worker_id_;
    size_t running_task_count_;
    std::chrono::system_clock::time_point  last_heartbeat_time_;//只记录最后一次Heartbeat
    bool alive_;
public:
int getWorkeId()const;
size_t getRunningTaskCount() const;

void updateHeartbeat();
void increaseTaskCount();
void decreaseTaskCount();
bool isAlive() const;
void markDead();

};

}