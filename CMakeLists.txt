# ============================================================
# CMakeLists.txt - 分布式任务调度系统
# ============================================================
# 
# 项目阶段：核心开发 + 测试验证
# 
# 构建目标：
#   1. 将 src/ 下所有核心代码编译成静态库 dts_core
#   2. 将测试程序分别链接 dts_core
# 
# 使用方式：
#   mkdir build && cd build
#   cmake ..
#   make
#   ./tests/protocol/protocol_test
#   ./tests/integration/register_test
# ============================================================
cmake_minimum_required(VERSION 3.15)

project(DistributedTaskScheduler)
enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# =========================
# Header files
# =========================

include_directories(include)


# =========================
# Core library
# =========================

set(DTS_CORE_SRC

    # common
    src/common/Protocol.cpp
    src/common/Task.cpp
    src/common/WorkerInfo.cpp

    # network
    src/network/Connection.cpp
    src/network/TCPClient.cpp
    src/network/TCPServer.cpp
    src/network/FrameDecoder.cpp
    src/network/EpollPoller.cpp
    src/network/TCPReactor.cpp

    # master
    src/master/Master.cpp
    src/master/Scheduler.cpp
    src/master/TaskManager.cpp
    src/master/WorkerManager.cpp

    # worker
    src/worker/TaskExecutor.cpp
    src/worker/TaskDeduplicator.cpp
    src/worker/Worker.cpp

    #client
    src/client/Client.cpp
)


add_library(dts_core STATIC
    ${DTS_CORE_SRC}
)

# =========================
# Applications
# =========================

add_executable(dts_master
    apps/master_main.cpp
)

target_link_libraries(dts_master
    dts_core
)
add_executable(dts_worker
    apps/worker_main.cpp
)

target_link_libraries(dts_worker
    dts_core
)
add_executable(dts_client
    apps/client_main.cpp
)

target_link_libraries(dts_client
    dts_core
)
add_executable(dts_connection_benchmark
    apps/connection_benchmark_main.cpp
)

target_link_libraries(dts_connection_benchmark
    dts_core
)
# =========================
# Tests
# =========================

# protocol test
add_executable(protocol_test
    tests/protocol/protocol_test.cpp
)

target_link_libraries(protocol_test
    dts_core
)

# integration test
add_executable(register_test
    tests/integration/register_test.cpp
)

target_link_libraries(register_test
    dts_core
)
add_executable(task_manager_test
    tests/master/task_manager_test.cpp
)

target_link_libraries(task_manager_test
    dts_core
)

add_executable(worker_manager_test
    tests/master/worker_manager_test.cpp
)

target_link_libraries(worker_manager_test
    dts_core
)

add_executable(task_e2e_test
    tests/integration/task_e2e_test.cpp
)

target_link_libraries(task_e2e_test
    dts_core
)

add_executable(task_retry_e2e_test
    tests/integration/task_retry_e2e_test.cpp
)

target_link_libraries(task_retry_e2e_test
    dts_core
)
add_executable(heartbeat_timeout_e2e_test
    tests/integration/heartbeat_timeout_e2e_test.cpp
)

target_link_libraries(heartbeat_timeout_e2e_test
    dts_core
)

add_executable(no_worker_task_e2e_test
    tests/integration/no_worker_task_e2e_test.cpp
)

target_link_libraries(no_worker_task_e2e_test
    dts_core
)
add_executable(scheduler_failure_test
    tests/master/scheduler_failure_test.cpp
)

target_link_libraries(scheduler_failure_test
    dts_core
)
add_executable(connection_framing_test
    tests/network/connection_framing_test.cpp
)

target_link_libraries(connection_framing_test
    dts_core
)
add_executable(graceful_stop_test
    tests/integration/graceful_stop_test.cpp
)

target_link_libraries(graceful_stop_test
    dts_core
)
add_executable(worker_timeout_recovery_e2e_test
    tests/integration/worker_timeout_recovery_e2e_test.cpp
)
target_link_libraries(worker_timeout_recovery_e2e_test
    dts_core
)


add_executable(connection_concurrent_send_test
    tests/network/connection_concurrent_send_test.cpp
)

target_link_libraries(connection_concurrent_send_test
    dts_core
)
add_executable(query_task_test
    tests/integration/query_task_test.cpp
)

target_link_libraries(query_task_test
    dts_core
)
add_executable(worker_thread_pool_test
    tests/integration/worker_thread_pool_test.cpp
)
target_link_libraries(worker_thread_pool_test
    dts_core
)
add_executable(stale_execution_result_test
    tests/master/stale_execution_result_test.cpp
)

target_link_libraries(stale_execution_result_test
    dts_core
)
add_executable(task_deduplicator_test
    tests/worker/task_deduplicator_test.cpp
)

target_link_libraries(task_deduplicator_test
    dts_core
)
add_executable(worker_dedup_test
    tests/worker/worker_dedup_test.cpp
)

target_link_libraries(worker_dedup_test
    dts_core
)
add_executable(dts_benchmark
    apps/benchmark_main.cpp
)

target_link_libraries(dts_benchmark
    dts_core
)
add_executable(frame_decoder_test
    tests/network/frame_decoder_test.cpp
)

target_link_libraries(frame_decoder_test
    dts_core
)
add_executable(nonblocking_connection_test
    tests/network/nonblocking_connection_test.cpp
)

target_link_libraries(nonblocking_connection_test
    dts_core
)
add_executable(epoll_poller_test
    tests/network/epoll_poller_test.cpp
)

target_link_libraries(epoll_poller_test
    dts_core
)

add_executable(nonblocking_accept_test
    tests/network/nonblocking_accept_test.cpp
)

target_link_libraries(nonblocking_accept_test
    dts_core
)
add_executable(tcp_reactor_test
    tests/network/tcp_reactor_test.cpp
)

target_link_libraries(tcp_reactor_test
    dts_core
)
add_executable(nonblocking_output_test
    tests/network/nonblocking_output_test.cpp
)

target_link_libraries(nonblocking_output_test
    PRIVATE
    dts_core
)
add_executable(reactor_half_close_test
    tests/network/reactor_half_close_test.cpp
)

target_link_libraries(reactor_half_close_test
    PRIVATE
    dts_core
)
add_executable(reactor_peer_close_test
    tests/network/reactor_peer_close_test.cpp
)

target_link_libraries(reactor_peer_close_test
    dts_core
)
add_executable(dts_request_benchmark
    apps/request_benchmark_main.cpp
)

target_link_libraries(dts_request_benchmark
    dts_core
)
# ========================
# CTest registration
# ========================

add_test(
    NAME protocol_test
    COMMAND protocol_test
)
add_test(
    NAME register_test
    COMMAND register_test
)

add_test(
    NAME task_manager_test
    COMMAND task_manager_test
)

add_test(
    NAME worker_manager_test
    COMMAND worker_manager_test
)
add_test(
    NAME task_e2e_test
    COMMAND task_e2e_test
)
set_tests_properties(
    task_e2e_test 
    PROPERTIES TIMEOUT 10
)

add_test(
    NAME task_retry_e2e_test
    COMMAND task_retry_e2e_test
)
set_tests_properties(
    task_retry_e2e_test 
    PROPERTIES TIMEOUT 10
)

add_test(
    NAME heartbeat_timeout_e2e_test
    COMMAND heartbeat_timeout_e2e_test
)
set_tests_properties(
    heartbeat_timeout_e2e_test 
    PROPERTIES TIMEOUT 30
)

add_test(
    NAME no_worker_task_e2e_test
    COMMAND no_worker_task_e2e_test
)
set_tests_properties(
    no_worker_task_e2e_test 
    PROPERTIES TIMEOUT 10
)

add_test(
    NAME scheduler_failure_test
    COMMAND scheduler_failure_test
)

add_test(
    NAME connection_framing_test
    COMMAND connection_framing_test
)
set_tests_properties(
    connection_framing_test 
    PROPERTIES TIMEOUT 5
)


add_test(
    NAME graceful_stop_test
    COMMAND graceful_stop_test
)
set_tests_properties(
    graceful_stop_test 
    PROPERTIES TIMEOUT 5
)

add_test(
    NAME worker_timeout_recovery_e2e_test
    COMMAND worker_timeout_recovery_e2e_test
)
set_tests_properties(
    worker_timeout_recovery_e2e_test
    PROPERTIES TIMEOUT 30
)

add_test(
    NAME connection_concurrent_send_test
    COMMAND connection_concurrent_send_test
)

set_tests_properties(
    connection_concurrent_send_test
    PROPERTIES TIMEOUT 5
)
add_test(
    NAME query_task_test
    COMMAND query_task_test
)

set_tests_properties(
    query_task_test
    PROPERTIES TIMEOUT 10
)
add_test(
    NAME worker_thread_pool_test
    COMMAND worker_thread_pool_test
)

set_tests_properties(
    worker_thread_pool_test
    PROPERTIES TIMEOUT 10
)
add_test(
    NAME stale_execution_result_test
    COMMAND stale_execution_result_test
)
add_test(
    NAME task_deduplicator_test
    COMMAND task_deduplicator_test
)

set_tests_properties(
    task_deduplicator_test
    PROPERTIES TIMEOUT 5
)
add_test(
    NAME worker_dedup_test
    COMMAND worker_dedup_test
)

set_tests_properties(
    worker_dedup_test
    PROPERTIES TIMEOUT 5
)
add_test(
    NAME frame_decoder_test
    COMMAND frame_decoder_test
)

set_tests_properties(
    frame_decoder_test
    PROPERTIES TIMEOUT 5
)
add_test(
    NAME nonblocking_connection_test
    COMMAND nonblocking_connection_test
)

set_tests_properties(
    nonblocking_connection_test
    PROPERTIES TIMEOUT 5
)
add_test(
    NAME epoll_poller_test
    COMMAND epoll_poller_test
)

set_tests_properties(
    epoll_poller_test
    PROPERTIES TIMEOUT 5
)
add_test(
    NAME nonblocking_accept_test
    COMMAND nonblocking_accept_test
)
set_tests_properties(
    nonblocking_accept_test
    PROPERTIES TIMEOUT 5
)
add_test(
    NAME tcp_reactor_test
    COMMAND tcp_reactor_test
)

set_tests_properties(
    tcp_reactor_test
    PROPERTIES TIMEOUT 5
)
add_test(
    NAME nonblocking_output_test
    COMMAND nonblocking_output_test
)

set_tests_properties(
    nonblocking_output_test
    PROPERTIES TIMEOUT 5
)
add_test(
    NAME reactor_half_close_test
    COMMAND reactor_half_close_test
)

set_tests_properties(
    reactor_half_close_test
    PROPERTIES TIMEOUT 5
)
add_test(
    NAME reactor_peer_close_test
    COMMAND reactor_peer_close_test
)