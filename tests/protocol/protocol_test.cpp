/*
 * protocol_test.cpp
 * Protocol 层单元测试
 * 测试序列化/反序列化是否正确，不涉及网络
 */

#include <iostream>
#include <cassert>
#include "../../include/common/Protocol.h"

using namespace dts;

void testMessageSerializeDeserialize() {
    std::cout << "=== Test 1: Message 序列化/反序列化 ===" << std::endl;
    
    // 1. 构造原始 Message
    Message original;
    original.header.type = MessageType::REGISTER_WORKER;
    original.data = "1|192.168.1.100|8080";
    original.header.length = original.data.size();
    
    // 2. 序列化
    std::string raw = Protocol::serialize(original);
    std::cout << "Serialized: " << raw << std::endl;
    
    // 3. 反序列化
    Message parsed = Protocol::deserialize(raw);
    
    // 4. 验证
    assert(parsed.header.type == original.header.type);
    assert(parsed.header.length == original.header.length);
    assert(parsed.data == original.data);
    
    std::cout << "✅ Message 序列化/反序列化测试通过!" << std::endl;
    std::cout << "   Type: " << Protocol::messageTypeToString(parsed.header.type) << std::endl;
    std::cout << "   Length: " << parsed.header.length << std::endl;
    std::cout << "   Data: " << parsed.data << std::endl;
    std::cout << std::endl;
}

void testWorkerRegisterInfo() {
    std::cout << "=== Test 2: WorkerRegisterInfo 序列化/反序列化 ===" << std::endl;
    
    // 1. 构造原始数据
    WorkerRegisterInfo original;
    original.worker_id = 1;
    original.ip = "192.168.1.100";
    original.port = 8080;
    
    // 2. 序列化
    std::string serialized = Protocol::serializeWorkerInfo(original);
    std::cout << "Serialized WorkerInfo: " << serialized << std::endl;
    
    // 3. 反序列化
    WorkerRegisterInfo parsed = Protocol::deserializeWorkerInfo(serialized);
    
    // 4. 验证
    assert(parsed.worker_id == original.worker_id);
    assert(parsed.ip == original.ip);
    assert(parsed.port == original.port);
    
    std::cout << "✅ WorkerRegisterInfo 序列化/反序列化测试通过!" << std::endl;
    std::cout << "   Worker ID: " << parsed.worker_id << std::endl;
    std::cout << "   IP: " << parsed.ip << std::endl;
    std::cout << "   Port: " << parsed.port << std::endl;
    std::cout << std::endl;
}

void testFullMessageWithWorkerInfo() {
    std::cout << "=== Test 3: 完整消息闭环 (Message + WorkerInfo) ===" << std::endl;
    
    // 1. 构造 Worker 注册信息
    WorkerRegisterInfo info;
    info.worker_id = 2;
    info.ip = "10.0.0.50";
    info.port = 9090;
    
    // 2. 序列化 WorkerInfo → data
    std::string data = Protocol::serializeWorkerInfo(info);
    
    // 3. 构造 Message
    Message msg;
    msg.header.type = MessageType::REGISTER_WORKER;
    msg.data = data;
    msg.header.length = msg.data.size();
    
    std::cout << "原始 Message:" << std::endl;
    std::cout << "  Type: " << Protocol::messageTypeToString(msg.header.type) << std::endl;
    std::cout << "  Data: " << msg.data << std::endl;
    
    // 4. 序列化 Message → 发送字符串
    std::string raw = Protocol::serialize(msg);
    std::cout << "发送字符串: " << raw << std::endl;
    
    // 5. 反序列化字符串 → Message
    Message received = Protocol::deserialize(raw);
    
    // 6. 验证 Message 层
    assert(received.header.type == MessageType::REGISTER_WORKER);
    assert(received.data == data);
    
    // 7. 反序列化 data → WorkerRegisterInfo
    WorkerRegisterInfo parsed = Protocol::deserializeWorkerInfo(received.data);
    assert(parsed.worker_id == info.worker_id);
    assert(parsed.ip == info.ip);
    assert(parsed.port == info.port);
    
    std::cout << "✅ 完整消息闭环测试通过!" << std::endl;
    std::cout << "   收到的 Worker ID: " << parsed.worker_id << std::endl;
    std::cout << "   收到的 IP: " << parsed.ip << std::endl;
    std::cout << "   收到的 Port: " << parsed.port << std::endl;
    std::cout << "   Message Type: " << Protocol::messageTypeToString(received.header.type) << std::endl;
    std::cout << std::endl;
}

void testEnumToString() {
    std::cout << "=== Test 4: 枚举转字符串 ===" << std::endl;
    
    assert(Protocol::messageTypeToString(MessageType::SUBMIT_TASK) == "SUBMIT_TASK");
    assert(Protocol::messageTypeToString(MessageType::REGISTER_WORKER) == "REGISTER_WORKER");
    assert(Protocol::messageTypeToString(MessageType::HEARTBEAT) == "HEARTBEAT");
    assert(Protocol::messageTypeToString(MessageType::TASK_RESULT) == "TASK_RESULT");
    assert(Protocol::messageTypeToString(MessageType::TASK_ASSIGN) == "TASK_ASSIGN");
    
    assert(Protocol::stringToMessageType("SUBMIT_TASK") == MessageType::SUBMIT_TASK);
    assert(Protocol::stringToMessageType("REGISTER_WORKER") == MessageType::REGISTER_WORKER);
    assert(Protocol::stringToMessageType("UNKNOWN") == MessageType::UNKNOWN);
    
    std::cout << "✅ 枚举转字符串测试通过!" << std::endl;
    std::cout << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Protocol 层单元测试" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    try {
        testEnumToString();
        testWorkerRegisterInfo();
        testMessageSerializeDeserialize();
        testFullMessageWithWorkerInfo();
        
        std::cout << "========================================" << std::endl;
        std::cout << "  ✅ 所有测试通过！" << std::endl;
        std::cout << "========================================" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 测试失败: " << e.what() << std::endl;
        return 1;
    }
}