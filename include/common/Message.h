/*
 * Message.h
 * 消息相关定义，包括消息类型和消息结构体
 */
#pragma once
#include<string>
#include <cstdint>
namespace dts{
enum class MessageType:uint16_t 
{
    // Client → Master
    SUBMIT_TASK = 1,
    
    // Worker → Master
    REGISTER_WORKER = 10,
    HEARTBEAT = 11,
    TASK_RESULT = 12,
    
    // Master → Worker
    TASK_ASSIGN = 20,

    UNKNOWN=0
};
struct MessageHeader{
    uint32_t length;//body长度,length=data.size()
    MessageType type; 
};

struct Message{
MessageHeader header;
std::string data;

};
}