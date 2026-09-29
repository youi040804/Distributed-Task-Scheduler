#include <atomic>     
#include <cassert>     
#include <chrono>     
#include <iostream>    
#include <thread>     
#include <vector>      

#include "worker/TaskDeduplicator.h"

using namespace dts;

void testSuccessfulResultCache() {
    std::cout << "=== Test 1: successful result cache ===" << std::endl;

    TaskDeduplicator deduplicator;

    // 第一次看到 task 1，应获得执行权
    assert(deduplicator.acquire(1));

    deduplicator.markSucceeded(1, "success");

    // 已成功完成，不应该再次获得执行权
    assert(!deduplicator.acquire(1));

    auto result = deduplicator.getSuccessfulResult(1);

    assert(result.has_value());
    assert(*result == "success");

    std::cout << "success result cache passed" << std::endl;
}

void testFailedTaskCanRetry() {
    std::cout << "=== Test 2: failed task can retry ===" << std::endl;

    TaskDeduplicator deduplicator;

    assert(deduplicator.acquire(1));

    // 第一次执行失败
    deduplicator.markFailed(1);

    // FAILED 不缓存，因此新的 execution应该能够重新获得执行权
    assert(deduplicator.acquire(1));

    deduplicator.markSucceeded(1, "retry-success");

    auto result = deduplicator.getSuccessfulResult(1);

    assert(result.has_value());
    assert(*result == "retry-success");

    std::cout << "failed task retry passed" << std::endl;
}

void testConcurrentSingleFlight() {
    std::cout << "=== Test 3: concurrent single-flight ===" << std::endl;

    TaskDeduplicator deduplicator;

    constexpr int thread_count = 8;

    std::atomic<int> actual_execution_count{0};
    std::atomic<int> cached_result_count{0};

    std::vector<std::thread> threads;
    threads.reserve(thread_count);

    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back([&]() {
            const bool should_execute = deduplicator.acquire(42);

            if (should_execute) {
                // 只有获得执行权的线程才能走这里
                actual_execution_count.fetch_add(1);

                // 模拟真正业务执行
                std::this_thread::sleep_for( std::chrono::milliseconds(100) );

                deduplicator.markSucceeded( 42, "task-42-result" );

                return;
            }

            // 其他线程必须等第一个线程成功后，再从 cache 中拿到相同结果
            auto result = deduplicator.getSuccessfulResult(42);

            assert(result.has_value());
            assert(*result == "task-42-result");

            cached_result_count.fetch_add(1);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // 8 个线程同时处理同一个逻辑任务，真正执行只能发生一次
    assert(actual_execution_count.load() == 1);

    // 剩余 7 个线程全部复用缓存结果
    assert( cached_result_count.load() == thread_count - 1 );

    std::cout
        << "single-flight passed: actual execution = "
        << actual_execution_count.load()
        << ", cached = "
        << cached_result_count.load()
        << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << " TaskDeduplicator Test" << std::endl;
    std::cout << "========================================" << std::endl;

    testSuccessfulResultCache();
    testFailedTaskCanRetry();
    testConcurrentSingleFlight();

    std::cout << "========================================" << std::endl;
    std::cout << " All TaskDeduplicator tests passed" << std::endl;

    return 0;
}