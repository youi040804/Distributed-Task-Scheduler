/*
 * Message.h
 * 消息相关定义，包括消息类型和消息结构体
 */
#pragma once
#include<string>

namespace dts{
enum class MessageType{
    // Client → Master
    SUBMIT_TASK = 1,
    
    // Worker → Master
    REGISTER_WORKER = 10,
    HEARTBEAT = 11,
    TASK_RESULT = 12,
    
    // Master → Worker
    TASK_ASSIGN = 20
};

struct Message{
MessageType type;
std::string data;

};
}