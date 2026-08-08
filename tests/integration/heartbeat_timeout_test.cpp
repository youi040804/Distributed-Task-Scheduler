/*
 * heartbeat_timeout_test.cpp
 * 集成测试：测试 Master 心跳超时检测
 * 场景：Worker 注册 → 发送一次心跳 → 等待超时 → Master 标记死亡
 */

#include <iostream>
#include <thread>
#include <chrono>
#include "../../include/master/Master.h"
#include "../../include/worker/Worker.h"
#include "../../include/common/Protocol.h"
#include "../../include/network/TCPClient.h"
#include "../../include/utils/Config.h"

using namespace dts;

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Heartbeat 超时检测集成测试" << std::endl;
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
    std::cout << "   超时阈值: " << HEARTBEAT_TIMEOUT << " 秒" << std::endl;
    std::cout << std::endl;

    // 2. 在后台线程运行 Master 主循环
    std::cout << "[Step 2] 启动 Master 主循环线程..." << std::endl;
    std::thread master_thread([&master]() {
        master.run();
    });
    std::cout << "✅ Master 主循环线程已启动" << std::endl;
    std::cout << std::endl;

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

    // 等待 Master 处理注册
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 4. 验证 Worker 已注册
    std::cout << "[Step 4] 验证 Worker 已注册..." << std::endl;
    auto info_before = master.getWorkerInfo(1);
    if (!info_before.has_value()) {
        std::cerr << "Worker 未在 Manager 中找到！" << std::endl;
        return 1;
    }
    std::cout << "✅ Worker 已注册，初始状态:" << std::endl;
    std::cout << "   alive = " << (info_before->isAlive() ? "true" : "false") << std::endl;
    std::cout << std::endl;

    // 5. Worker 发送一次心跳
    std::cout << "[Step 5] Worker 发送一次心跳..." << std::endl;
    if (!worker.sendHeartbeat()) {
        std::cerr << "发送心跳失败！" << std::endl;
        return 1;
    }
    std::cout << "✅ 心跳已发送" << std::endl;
    std::cout << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // 6. 验证心跳已更新
    std::cout << "[Step 6] 验证心跳已更新..." << std::endl;
    auto info_after_heartbeat = master.getWorkerInfo(1);
    if (!info_after_heartbeat.has_value()) {
        std::cerr << "Worker 未在 Manager 中找到！" << std::endl;
        return 1;
    }
    std::cout << "✅ 心跳已更新，当前状态:" << std::endl;
    std::cout << "   alive = " << (info_after_heartbeat->isAlive() ? "true" : "false") << std::endl;
    std::cout << std::endl;

    // 7. 等待超时（超时阈值 10 秒 + 检测间隔 3 秒）
    std::cout << "[Step 7] 等待心跳超时..." << std::endl;
    std::cout << "   超时阈值: " << HEARTBEAT_TIMEOUT << " 秒" << std::endl;
    std::cout << "   检测间隔: " << HEARTBEAT_CHECK_INTERVAL << " 秒" << std::endl;
    std::cout << "   等待中..." << std::endl;
    
    int waitTime = HEARTBEAT_TIMEOUT + HEARTBEAT_CHECK_INTERVAL + 3;  // 10 + 3 + 3 = 16
    for (int i = 0; i < waitTime; i++) {
        std::cout << "\r  已等待 " << (i + 1) << " / " << waitTime << " 秒" << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << std::endl;
    std::cout << "✅ 等待完成" << std::endl;
    std::cout << std::endl;

    // 8. 验证 Worker 已被标记为死亡
    std::cout << "[Step 8] 验证 Worker 状态..." << std::endl;
    auto info_final = master.getWorkerInfo(1);
    if (!info_final.has_value()) {
        std::cerr << "Worker 未在 Manager 中找到！" << std::endl;
        return 1;
    }

    if (!info_final->isAlive()) {
        std::cout << "✅ Worker 已被标记为死亡 (alive = false)" << std::endl;
    } else {
        std::cout << "❌ Worker 仍为存活状态 (alive = true)，超时检测失败！" << std::endl;
        return 1;
    }
    std::cout << std::endl;

    // 9. 清理
    std::cout << "[Step 9] 清理资源..." << std::endl;
    master.stop();
    if (master_thread.joinable()) {
        master_thread.join();
    }
    std::cout << "✅ 清理完成" << std::endl;
    std::cout << std::endl;

    std::cout << "========================================" << std::endl;
    std::cout << "  ✅ 所有测试通过！" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}