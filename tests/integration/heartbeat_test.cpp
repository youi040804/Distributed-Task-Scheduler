/*
 * heartbeat_test.cpp
 * 集成测试：测试 Worker 心跳发送和 Master 接收处理
 * 验证心跳链路：Worker → Master → WorkerManager 更新
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <unistd.h>
#include "../include/master/Master.h"
#include "../include/worker/Worker.h"
#include "../include/common/Protocol.h"
#include "../include/network/TCPClient.h"

using namespace dts;

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Heartbeat 集成测试" << std::endl;
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

    // 3. Worker 注册
    std::cout << "[Step 3] Worker 注册..." << std::endl;
    Worker worker(1);
    if (!worker.start("127.0.0.1", 8080, "127.0.0.1", 9000)) {
        std::cerr << "Worker 注册失败！" << std::endl;
        return 1;
    }
    std::cout << "✅ Worker 注册成功" << std::endl;
    std::cout << std::endl;

    // 等待 Master 处理注册消息
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 4. 获取注册后的心跳时间
    std::cout << "[Step 4] 获取注册后的心跳时间..." << std::endl;
    const WorkerInfo* info_before = master.getWorkerInfo(1);
    if (info_before == nullptr) {
        std::cerr << "Worker 未在 Manager 中找到！" << std::endl;
        return 1;
    }
    auto time_before = info_before->getLastHeartbeatTime();
    std::cout << "✅ 注册后心跳时间: " << time_before.time_since_epoch().count() << std::endl;
    std::cout << std::endl;

    // 5. Worker 发送心跳
    std::cout << "[Step 5] Worker 发送心跳..." << std::endl;
    if (!worker.sendHeartbeat()) {
        std::cerr << "发送心跳失败！" << std::endl;
        return 1;
    }
    std::cout << "✅ 心跳消息已发送" << std::endl;
    std::cout << std::endl;

    // 等待 Master 处理心跳
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 6. 验证心跳时间已更新
    std::cout << "[Step 6] 验证心跳时间已更新..." << std::endl;
    const WorkerInfo* info_after = master.getWorkerInfo(1);
    if (info_after == nullptr) {
        std::cerr << "Worker 未在 Manager 中找到！" << std::endl;
        return 1;
    }
    auto time_after = info_after->getLastHeartbeatTime();

    std::cout << "   心跳后时间: " << time_after.time_since_epoch().count() << std::endl;

    if (time_after > time_before) {
        std::cout << "✅ 心跳时间已更新！" << std::endl;
    } else {
        std::cout << "❌ 心跳时间未更新！" << std::endl;
        return 1;
    }
    std::cout << std::endl;

    // 7. 验证 Worker 存活状态
    std::cout << "[Step 7] 验证 Worker 存活状态..." << std::endl;
    if (info_after->isAlive()) {
        std::cout << "✅ Worker 存活状态: 存活" << std::endl;
    } else {
        std::cout << "❌ Worker 存活状态: 已死亡（错误！）" << std::endl;
        return 1;
    }
    std::cout << std::endl;

    // 8. 清理
    std::cout << "[Step 8] 清理资源..." << std::endl;
    master_thread.detach();
    std::cout << "✅ 清理完成" << std::endl;
    std::cout << std::endl;

    std::cout << "========================================" << std::endl;
    std::cout << "  ✅ 所有测试通过！" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}