#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

#include "worker/Worker.h"
#include "common/Protocol.h"
#include "network/TCPServer.h"
#include "network/TCPClient.h"

using namespace dts;

int main()
{
    constexpr int TEST_PORT = 19003;
    constexpr int WORKER_ID = 1;

    std::cout << "========== Worker Task Execution Test =========="
              << std::endl;

    // ------------------------------------------------------------
    // 1. 启动假的 Master TCP Server
    // ------------------------------------------------------------
    TCPServer master_server(TEST_PORT);

    assert(master_server.start());

    std::cout << "[Test] Fake Master server started."
              << std::endl;

    // ------------------------------------------------------------
    // 2. 创建 Worker
    // ------------------------------------------------------------
    Worker worker(WORKER_ID);

    // ------------------------------------------------------------
    // 3. Worker 连接假的 Master
    // ------------------------------------------------------------
    std::thread worker_thread([&worker]() {
        bool result = worker.start(
            "127.0.0.1",
            TEST_PORT,
            "127.0.0.1",
            20001
        );

        assert(result);
    });

    // ------------------------------------------------------------
    // 4. Fake Master 接收 Worker 连接
    // ------------------------------------------------------------
    auto connection = master_server.acceptConnection();

    assert(connection != nullptr);

    std::cout << "[Test] Worker connected to fake Master."
              << std::endl;

    // Worker 注册消息
    Message register_msg = connection->receiveMessage();

    assert(register_msg.header.type == MessageType::REGISTER_WORKER);

    std::cout << "[Test] Worker registration received."
              << std::endl;

    // ------------------------------------------------------------
    // 5. 等待 Worker 启动线程
    // ------------------------------------------------------------
    worker_thread.join();

    std::cout << "[Test] Worker threads started."
              << std::endl;

    // ------------------------------------------------------------
    // 6. 构造 TASK_ASSIGN
    // ------------------------------------------------------------
    TaskAssignInfo task_info;

    task_info.task_id = 100;
    task_info.payload = "hello worker";

    Message assign_msg;

    assign_msg.header.type = MessageType::TASK_ASSIGN;
    assign_msg.data =
        Protocol::serializeTaskAssignInfo(task_info);

    // ------------------------------------------------------------
    // 7. Fake Master 向 Worker 发送任务
    // ------------------------------------------------------------
    assert(connection->sendMessage(assign_msg));

    std::cout << "[Test] TASK_ASSIGN sent to Worker."
              << std::endl;

    // ------------------------------------------------------------
    // 8. 等待 Worker 执行任务
    // ------------------------------------------------------------
    Message result_msg = connection->receiveMessage();

    assert(result_msg.header.type == MessageType::TASK_RESULT);

    std::cout << "[Test] TASK_RESULT received from Worker."
              << std::endl;

    // ------------------------------------------------------------
    // 9. 解析 TASK_RESULT
    // ------------------------------------------------------------
    TaskResultInfo result =
        Protocol::deserializeTaskResultInfo(result_msg.data);

    assert(result.task_id == 100);
    assert(result.status == TaskStatus::DONE);

    std::cout << "[Test] Result task ID = "
              << result.task_id
              << std::endl;

    std::cout << "[Test] Result status = DONE"
              << std::endl;

    std::cout << "[Test] Result payload = "
              << result.payload
              << std::endl;

    // ------------------------------------------------------------
    // 10. 验证结果内容
    // ------------------------------------------------------------
    assert(
        result.payload ==
        "Task100 executed successfully"
    );

    std::cout << "[Test] Task execution result verified."
              << std::endl;

    // ------------------------------------------------------------
    // 11. 清理 Worker
    // ------------------------------------------------------------
    worker.stop();

    master_server.stop();

    std::cout << "==============================================="
              << std::endl;

    std::cout << "Worker Task Execution Test PASSED!"
              << std::endl;

    return 0;
}