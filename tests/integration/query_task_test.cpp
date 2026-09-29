/*
 * query_task_test.cpp
 */

#include <cassert>      
#include <chrono>       
#include <iostream>     
#include <thread>       

#include "client/Client.h"
#include "common/Protocol.h"
#include "master/Master.h"

int main() {
    // 使用独立端口，避免和其他测试或手工运行的 Master 冲突
    constexpr int kMasterPort = 19001;

    dts::Master master(kMasterPort);

    // 1. 启动 Master
    if (!master.start()) {
        std::cerr
            << "[query_task_test] failed to start Master"
            << std::endl;
        return 1;
    }

    // Master::run() 会阻塞在 acceptConnection()，
    // 因此放到独立线程运行。
    std::thread master_thread([&master]() {
        master.run();
    });

    // 给 Master 一点时间进入监听状态。
    std::this_thread::sleep_for(
        std::chrono::milliseconds(100)
    );

    // 2. 创建并连接 Client
    dts::Client client(1);

    client.setMasterAddress(
        "127.0.0.1",
        kMasterPort
    );

    if (!client.connectMaster()) {
        std::cerr
            << "[query_task_test] failed to connect to Master"
            << std::endl;

        master.stop();

        if (master_thread.joinable()) {
            master_thread.join();
        }

        return 1;
    }

    // ========================================================
    // Case 1:
    // 提交一个任务，然后使用返回的 task_id 查询
    // ========================================================

    dts::TaskSubmitInfo task;
    task.priority = 5;
    task.payload = "query-test";

    auto task_id = client.submitTask(task);

    assert(task_id.has_value());
    assert(*task_id > 0);

    std::cout
        << "[query_task_test] submitted task_id="
        << *task_id
        << std::endl;

    auto status = client.queryTask(*task_id);

    // 查询请求本身成功
    assert(status.has_value());

    // 任务应该存在
    assert(status->found);

    // 返回的 task_id 应与提交时获得的一致
    assert(status->task_id == *task_id);

    /*
     * 本测试没有启动 Worker。
     *
     * Scheduler 即使取到任务，也找不到可用 Worker，
     * 会重新将任务放回 ready queue，因此任务应保持 PENDING。
     */
    assert(status->status == dts::TaskStatus::PENDING);

    std::cout
        << "[query_task_test] existing task query passed"
        << ", task_id="
        << status->task_id
        << std::endl;

    // ========================================================
    // Case 2:
    // 查询一个不存在的 task_id
    // ========================================================

    constexpr int kMissingTaskId = 999999;

    auto missing =
        client.queryTask(kMissingTaskId);

    // Master 正常响应，所以 optional 应该有值
    assert(missing.has_value());

    // 但任务不存在
    assert(!missing->found);

    assert(
        missing->task_id ==
        kMissingTaskId
    );

    std::cout
        << "[query_task_test] missing task query passed"
        << ", task_id="
        << missing->task_id
        << std::endl;

    // ========================================================
    // Cleanup
    // ========================================================

    client.stop();

    master.stop();

    if (master_thread.joinable()) {
        master_thread.join();
    }

    std::cout
        << "[query_task_test] all tests passed"
        << std::endl;

    return 0;
}