/*
 * register_test.cpp
 * 集成测试：测试 Worker 注册完整流程
 * 使用线程让 Master 在后台运行
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <unistd.h>
#include<cassert>
#include "master/Master.h"
#include "common/Protocol.h"
#include "network/TCPClient.h"

using namespace dts;

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Worker 注册流程集成测试" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // 1. 启动 Master
    std::cout << "[Step 1] 启动 Master..." << std::endl;
    Master master(8080);
    if (!master.start()) {
        std::cerr << "Master 启动失败！" << std::endl;
        return 1;
    }
    std::cout << "✅ Master 启动成功，监听端口 8080" << std::endl;
    std::cout << std::endl;

    // 2. 在后台线程运行 Master 主循环
    std::cout << "[Step 2] 启动 Master 主循环线程..." << std::endl;
    std::thread master_thread([&master]() {
        master.run();
    });
    std::cout << "✅ Master 主循环线程已启动" << std::endl;
    std::cout << std::endl;

    // 给 Master 一点时间启动
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 3. 构造 Worker 注册消息
    std::cout << "[Step 3] 构造 Worker 注册消息..." << std::endl;
    WorkerRegisterInfo info;
    info.worker_id = 1;
    info.ip = "127.0.0.1";
    info.port = 9000;

    std::string data = Protocol::serializeWorkerInfo(info);
    Message msg;
    msg.header.type = MessageType::REGISTER_WORKER;
    msg.data = data;

    std::cout << "   Worker ID: " << info.worker_id << std::endl;
    std::cout << "   Worker IP: " << info.ip << std::endl;
    std::cout << "   Worker Port: " << info.port << std::endl;
    std::cout << "✅ 注册消息构造完成" << std::endl;
    std::cout << std::endl;

    // 4. Worker 连接 Master
    std::cout << "[Step 4] Worker 连接 Master..." << std::endl;
    TCPClient client("127.0.0.1", 8080);
    if (!client.connect()) {
        std::cerr << "Worker 连接 Master 失败！" << std::endl;
        return 1;
    }
    std::cout << "✅ Worker 连接 Master 成功" << std::endl;
    std::cout << std::endl;

    // 5. Worker 发送注册消息
    std::cout << "[Step 5] Worker 发送注册消息..." << std::endl;
    Connection* conn = client.getConnection();
    if (!conn->sendMessage(msg)) {
        std::cerr << "发送注册消息失败！" << std::endl;
        return 1;
    }
    std::cout << "✅ 注册消息已发送" << std::endl;
    std::cout << std::endl;

    // 6. 等待 Master 处理消息
    std::cout << "[Step 6] 等待 Master 处理消息..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "✅ 等待完成" << std::endl;
    std::cout << std::endl;

    // 7. 验证 Worker 是否注册成功
    std::cout << "[Step 7] 验证注册结果..." << std::endl;
    auto stored = master.getWorkerInfo(1);
    if (stored.has_value()) {
        assert(stored->getWorkerId()==info.worker_id);
        assert(stored->getIp()==info.ip);
        assert(stored->getPort()==info.port);
        assert(stored->getRunningTaskCount()==0);
        assert(stored->isAlive());
       
    } else {
        std::cout << "❌ Worker 注册失败！Worker 未在 Manager 中找到" << std::endl;
        return 1;
    }
    std::cout << std::endl;

    // 8. 清理
    std::cout << "[Step 8] 清理资源..." << std::endl;
    // 这里需要让 Master 停止，但我们暂时没有 stop 方法
    // 简单起见，直接 detach 线程
    master.stop();
    if(master_thread.joinable()){
        master_thread.join();
    }
    std::cout << "✅ 清理完成" << std::endl;
    std::cout << std::endl;

    std::cout << "========================================" << std::endl;
    std::cout << "  ✅ 所有测试通过！" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}