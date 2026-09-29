#pragma once

#include <condition_variable> 
#include <mutex>             
#include <string>           
#include <unordered_map>     
#include <unordered_set> 
#include <optional>    
namespace dts {

class TaskDeduplicator {
private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::unordered_set<int> executing_tasks_;
    std::unordered_map<int, std::string> successful_results_;

public:

    bool acquire(int task_id);
    std::optional<std::string> getSuccessfulResult(int task_id);
    void markSucceeded( int task_id, const std::string& result );
    void markFailed(int task_id);
};

}