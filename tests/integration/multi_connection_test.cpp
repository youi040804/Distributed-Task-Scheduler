#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

#include "master/Master.h"
#include "network/TCPClient.h"
#include "common/Message.h"
#include "common/Protocol.h"

using namespace dts;

int main()
{
constexpr int MASTER_PORT = 19002;


std::cout << "========== Master Multi-Connection Test =========="
          << std::endl;

// ------------------------------------------------------------
// 1. 启动 Master
// ------------------------------------------------------------
Master master(MASTER_PORT);

assert(master.start());

std::cout << "[Test] Master started." << std::endl;

// Master::run() 是阻塞循环，因此放到后台线程
std::thread master_thread([&master]() {
    master.run();
});

// 给 Master 一点时间进入 accept()
std::this_thread::sleep_for(std::chrono::milliseconds(100));

// ------------------------------------------------------------
// 2. Worker 1 建立连接，但暂时什么都不发送
// ------------------------------------------------------------
TCPClient worker1_client("127.0.0.1", MASTER_PORT);

assert(worker1_client.connect());

std::cout << "[Test] Worker 1 connected." << std::endl;

// Worker 1 此时不发送任何消息。
// Master 对 Worker 1 的 handleConnection() 应该阻塞在 receiveMessage()
// 但不应该影响 Master::run() 继续 accept 新连接。

std::this_thread::sleep_for(std::chrono::milliseconds(200));

// ------------------------------------------------------------
// 3. Worker 2 建立连接
// ------------------------------------------------------------
TCPClient worker2_client("127.0.0.1", MASTER_PORT);

assert(worker2_client.connect());

std::cout << "[Test] Worker 2 connected." << std::endl;

// ------------------------------------------------------------
// 4. Worker 2 发送 REGISTER_WORKER
// ------------------------------------------------------------
WorkerRegisterInfo worker2_info;

worker2_info.worker_id = 2;
worker2_info.ip = "127.0.0.1";
worker2_info.port = 19012;

Message register_msg;

register_msg.header.type = MessageType::REGISTER_WORKER;
register_msg.data =
    Protocol::serializeWorkerInfo(worker2_info);

assert(
    worker2_client.getConnection()->sendMessage(register_msg)
);

std::cout << "[Test] Worker 2 registration sent."
          << std::endl;

// 给 Master 的 connection thread 一点处理时间
std::this_thread::sleep_for(std::chrono::milliseconds(200));

// ------------------------------------------------------------
// 5. 验证 Worker 2 已经被 Master 注册
// ------------------------------------------------------------
auto worker2 = master.getWorkerInfo(2);

assert(worker2.has_value());

std::cout << "[Test] Worker 2 registered successfully."
          << std::endl;

// ------------------------------------------------------------
// 6. 再次验证 Worker 1 和 Worker 2 可以同时存在
// ------------------------------------------------------------
auto worker1 = master.getWorkerInfo(1);

// Worker 1 没有发送 REGISTER_WORKER，
// 所以 WorkerManager 中不应该存在 Worker 1。
assert(!worker1.has_value());

std::cout << "[Test] Worker 1 has not registered yet."
          << std::endl;

std::cout << "[Test] Worker 2 was accepted even though "
             "Worker 1 connection was blocking."
          << std::endl;

// ------------------------------------------------------------
// 7. 清理
// ------------------------------------------------------------
worker1_client.getConnection()->disconnect();
worker2_client.getConnection()->disconnect();

master.stop();

// Master::run() 可能仍然阻塞在 acceptConnection()，
// 当前测试先等待一小段时间。
if (master_thread.joinable()) {
    master_thread.detach();
}

std::cout << "=============================================="
          << std::endl;

std::cout << "Master Multi-Connection Test PASSED!"
          << std::endl;

return 0;


}
