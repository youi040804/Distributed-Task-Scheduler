# ============================================================
# CMakeLists.txt - 分布式任务调度系统
# ============================================================
# 
# 项目阶段：核心开发 + 测试验证
# 
# 构建目标：
#   1. 将 src/ 下所有核心代码编译成静态库 dts_core
#   2. 将测试程序分别链接 dts_core
#   3. 暂时不编译 apps/（预留入口）
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
    src/common/Message.cpp
    src/common/Protocol.cpp
    src/common/Task.cpp
    src/common/WorkerInfo.cpp

    # network
    src/network/Connection.cpp
    src/network/TCPClient.cpp
    src/network/TCPServer.cpp

    # utils
    src/utils/Config.cpp
    src/utils/Logger.cpp
    src/utils/Timer.cpp

    # master
    src/master/Master.cpp
    src/master/Scheduler.cpp
    src/master/TaskManager.cpp
    src/master/WorkerManager.cpp

    # worker
    src/worker/TaskExecutor.cpp
    src/worker/Worker.cpp

    #client
    src/client/Client.cpp
)


add_library(dts_core STATIC
    ${DTS_CORE_SRC}
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


# network server test
add_executable(network_test_server
    tests/network/network_test_server.cpp
)

target_link_libraries(network_test_server
    dts_core
)


# network client test
add_executable(network_test_client
    tests/network/network_test_client.cpp
)

target_link_libraries(network_test_client
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
set_tests_properties(task_e2e_test PROPERTIES TIMEOUT 10)


add_test(
    NAME task_retry_e2e_test
    COMMAND task_retry_e2e_test
)
set_tests_properties(task_retry_e2e_test PROPERTIES TIMEOUT 10)

add_test(
    NAME heartbeat_timeout_e2e_test
    COMMAND heartbeat_timeout_e2e_test
)
set_tests_properties(heartbeat_timeout_e2e_test PROPERTIES TIMEOUT 30)

add_test(
    NAME no_worker_task_e2e_test
    COMMAND no_worker_task_e2e_test
)
set_tests_properties(no_worker_task_e2e_test PROPERTIES TIMEOUT 10)