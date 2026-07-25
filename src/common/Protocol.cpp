/*
*Protocol.cpp
*/
#include"../../include/common/Protocol.h"
namespace dts{

std::string Protocol::serialize(const Message&msg){
    return messageTypeToString(msg.type)+"|"+msg.data;

 }
    //反序列化：将string转成Message（用于接收）
Message Protocol::deserialize(const std::string&raw){
    size_t pos=raw.find("|");
    std::string type=raw.substr(0,pos);
    std::string data=raw.substr(pos+1);
    Message msg;
    msg.type=stringToMessageType(raw);
    msg.data=data;
    return msg;

}
    //辅助函数：WorkerRegisterInfo--string
std::string Protocol::serializeWorkerInfo(const WorkerRegisterInfo&info){
    MessageBuilder builder;
    builder<<info.worker_id<<info.ip<<info.port;
    return builder.str();
}

WorkerRegisterInfo Protocol:: deserializeWorkerInfo(const std::string&data){
    MessageParser parser(data);
    WorkerRegisterInfo workerinfo;
    parser>>workerinfo.worker_id;
    parser>>workerinfo.ip;
    parser>>workerinfo.port;
    return workerinfo;
    }
    //辅助函数：将枚举类转为字符串
std::string Protocol::messageTypeToString(MessageType type){
    switch(type){
        case dts::MessageType::SUBMIT_TASK:
            return "SUBMIT_TASK";
        case dts::MessageType::REGISTER_WORKER:
            return "REGISTER_WORKER";
        case dts::MessageType::TASK_ASSIGN:
            return "TASK_ASSIGN";
        case dts::MessageType::TASK_RESULT:
            return "TASK_RESULT";
        case dts::MessageType::HEARTBEAT:
            return "HEARTBEAT";
        default:
            return "UNKNOWN";
            
    }
}
    //辅助函数：将字符串转为枚举类
MessageType Protocol::stringToMessageType(const std::string&type){
    if (type == "SUBMIT_TASK") {
        return MessageType::SUBMIT_TASK;
    } else if (type == "REGISTER_WORKER") {
        return MessageType::REGISTER_WORKER;
    } else if (type == "TASK_ASSIGN") {
        return MessageType::TASK_ASSIGN;
    } else if (type == "TASK_RESULT") {
        return MessageType::TASK_RESULT;
    } else if (type == "HEARTBEAT") {
        return MessageType::HEARTBEAT;
    } else {
        return MessageType::UNKNOWN;  
    }
}

};

