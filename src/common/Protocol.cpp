/*
*Protocol.cpp
*序列化格式约定 ："MessageHeader|data"
*即：length|type|data
*例如："100|REGISTER_WORKER|1|192.168.1.100|8080"
*/
#include"common/Protocol.h"
namespace dts{

std::string Protocol::serialize(const Message&msg){
    uint32_t length=msg.data.size();
    std::string len=std::to_string(length);
    std::string type=messageTypeToString(msg.header.type);
    return len+"|"+type+"|"+msg.data;

}
    //反序列化：将string转成Message（用于接收）
Message Protocol::deserialize(const std::string&raw){
    Message msg;
    //MessageHeader header=msg.header;

    //截取length字段
    size_t pos1=raw.find("|");
    msg.header.length=static_cast<uint32_t>(std::stoul(raw.substr(0,pos1)));
    //截取type字段
    size_t pos2=raw.find("|",pos1+1);
    msg.header.type=stringToMessageType(raw.substr(pos1+1,pos2-(pos1+1)));
    //截取data字段
    msg.data=raw.substr(pos2+1);
 
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


//辅助函数：HeartbeatInfo--string
std::string Protocol::serializeHeartbeatInfo(const HeartbeatInfo&info){
    MessageBuilder builder;
    builder<<info.worker_id<<info.running_task_count;
    return builder.str();
}
HeartbeatInfo Protocol::deserializeHeartbeatInfo(const std::string&data){
    MessageParser parser(data);
    HeartbeatInfo heartbeatinfo;
    parser>>heartbeatinfo.worker_id;
    parser>>heartbeatinfo.running_task_count;
    return heartbeatinfo;
}
    
//辅助函数：TaskSubmitInfo--string
std::string Protocol::serializeTaskSubmitInfo(const TaskSubmitInfo&info){
    MessageBuilder builder;
    builder<<info.priority<<info.payload;
    return builder.str();
}
TaskSubmitInfo Protocol::deserializeTaskSubmitInfo(const std::string&data){
    MessageParser parser(data);
    TaskSubmitInfo task_submit_info;
    parser>>task_submit_info.priority;
    parser>>task_submit_info.payload;
    return task_submit_info;
}

//辅助函数：TaskAssignInfo--string
std::string Protocol::serializeTaskAssignInfo(const TaskAssignInfo&info){
    MessageBuilder builder;
    builder<<info.task_id<<info.payload;
    return builder.str();
}
TaskAssignInfo Protocol::deserializeTaskAssignInfo(const std::string&data){
    MessageParser parser(data);
    TaskAssignInfo task_assign_info;
    parser>>task_assign_info.task_id;
    parser>>task_assign_info.payload;
    return task_assign_info;
}

//辅助函数：TaskResultInfo--string
std::string Protocol::serializeTaskResultInfo(const TaskResultInfo&info){
    MessageBuilder builder;
    builder<<info.task_id<< static_cast<int>(info.status)<<info.payload;
    return builder.str();
}
TaskResultInfo Protocol::deserializeTaskResultInfo(const std::string&data){
    MessageParser parser(data);
    int status;

    TaskResultInfo task_result_info;
    parser>>task_result_info.task_id;
    
    parser >> status;
    task_result_info.status=static_cast<TaskStatus>(status);
    
    parser>>task_result_info.payload;
    return task_result_info;
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

