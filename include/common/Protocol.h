/*
Protocol序列化格式：
length|type|data
其中：
length = data长度
type = MessageType
data = 业务数据
*/
#pragma once
#include<string>
#include <sstream>//for ostringstream
#include<vector>
#include"Message.h"
namespace dts{
struct WorkerRegisterInfo
{
    int worker_id;
    std::string ip;
    int port;
};

struct HeartbeatInfo{
    int worker_id;
    int running_task_count;
};
struct TaskAssignInfo{
    int task_id;
    std::string payload;

};
//Protocol类
class Protocol{

public:
    //序列化，将Message转成string（用于发送）
    static std::string serialize(Message&msg);
    //反序列化：将string转成Message（用于接收）
    static Message deserialize(const std::string&raw);

    //辅助函数：WorkerRegisterInfo--string
    static std::string serializeWorkerInfo(const WorkerRegisterInfo&info);
    static WorkerRegisterInfo deserializeWorkerInfo(const std::string&data);

    //辅助函数：HeartbeatInfo--string
    static std::string serializeHeartbeatInfo(const HeartbeatInfo&info);
    static HeartbeatInfo deserializeHeartbeatInfo(const std::string&data);
    

    //辅助函数：将枚举类转为字符串
    static std::string messageTypeToString(MessageType type) ;
    //辅助函数：将字符串转为枚举类
    static MessageType stringToMessageType(const std::string&type);

    };

//序列化辅助类，采用链式拼接的方法
class MessageBuilder{
private:
    std::ostringstream oss;
    bool first=true;

public:
    template<typename T>
    MessageBuilder&operator<<(const T&value){
        if(!first) oss<<"|";
        oss<<value;
        first=false;
        return *this;
    }

    std::string str()const{
        return oss.str();
    }
};

//反序列化辅助类
class MessageParser{
private:
    std::vector<std::string> parts;
    size_t index=0;
public:
    //构造函数：接收data字符串，按 | 拆分
    MessageParser(const std::string&data){
        std::istringstream iss(data);
        std::string token;
        while(std::getline(iss,token,'|'))
        {
            parts.push_back(token);
        }
    }
    //链式取值
    template<typename T>
    MessageParser&operator>>(T &value){
        if(index<parts.size()){
            std::istringstream iss(parts[index++]);
            iss>>value;
        }
        return *this;
    }

        // 特化处理 string（防止string存在空格导致读取出现截断,所以直接赋值）
    MessageParser&operator>>(std::string&value){
        if(index<parts.size()){
            value=parts[index++];
        }
        return *this;
    }

};

}