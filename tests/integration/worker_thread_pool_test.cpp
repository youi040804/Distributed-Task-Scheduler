#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "client/Client.h"
#include "common/Protocol.h"
#include "master/Master.h"
#include "worker/Worker.h"

int main() {
    constexpr int kMasterPort = 19002;
    constexpr int kTaskCount = 4;
    constexpr size_t kExecutorThreads = 4;

    dts::Master master(kMasterPort);

    if (!master.start()) {
        std::cerr
            << "[worker_thread_pool_test] "
            << "failed to start Master\n";
        return 1;
    }

    std::thread master_thread(
        [&master]() {
            master.run();
        }
    );

    std::this_thread::sleep_for(
        std::chrono::milliseconds(100)
    );

    dts::Worker worker(
        1,
        kExecutorThreads
    );

    if (!worker.start(
            "127.0.0.1",
            kMasterPort,
            "127.0.0.1",
            0)) {

        std::cerr
            << "[worker_thread_pool_test] "
            << "failed to start Worker\n";

        master.stop();

        if (master_thread.joinable()) {
            master_thread.join();
        }

        return 1;
    }

    // 等待 Worker 注册完成。
    std::this_thread::sleep_for(
        std::chrono::milliseconds(200)
    );

    dts::Client client(1);

    client.setMasterAddress(
        "127.0.0.1",
        kMasterPort
    );

    if (!client.connectMaster()) {
        std::cerr
            << "[worker_thread_pool_test] "
            << "failed to connect Client\n";

        worker.stop();
        master.stop();

        if (master_thread.joinable()) {
            master_thread.join();
        }

        return 1;
    }

    std::vector<int> task_ids;

    const auto start =
        std::chrono::steady_clock::now();

    // 连续提交 4 个任务。
    for (int i = 0; i < kTaskCount; ++i) {
        dts::TaskSubmitInfo task;

        task.priority = 5;
        task.payload =
            "thread-pool-task-" +
            std::to_string(i);

        auto task_id =
            client.submitTask(task);

        assert(task_id.has_value());

        task_ids.push_back(*task_id);
    }

    bool all_done = false;

    // 最多等待 3 秒。
    // TaskExecutor 每个任务模拟执行约 1 秒。
    // 如果只有一个 executor，4 个任务通常需要约 4 秒；
    // 4 个 executor 应明显早于该时间完成。
    const auto deadline =
        start +
        std::chrono::seconds(3);

    while (std::chrono::steady_clock::now()
           < deadline) {

        all_done = true;

        for (int task_id : task_ids) {
            auto status =
                client.queryTask(task_id);

            assert(status.has_value());
            assert(status->found);

            if (status->status !=
                dts::TaskStatus::DONE) {

                all_done = false;
                break;
            }
        }

        if (all_done) {
            break;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(50)
        );
    }

    const auto end =
        std::chrono::steady_clock::now();

    const auto elapsed_ms =
        std::chrono::duration_cast<
            std::chrono::milliseconds
        >(end - start).count();

    std::cout
        << "[worker_thread_pool_test] "
        << kTaskCount
        << " tasks completed in "
        << elapsed_ms
        << " ms with "
        << kExecutorThreads
        << " executor threads"
        << std::endl;

    assert(all_done);

    client.stop();
    worker.stop();
    master.stop();

    if (master_thread.joinable()) {
        master_thread.join();
    }

    std::cout
        << "[worker_thread_pool_test] passed"
        << std::endl;

    return 0;
}