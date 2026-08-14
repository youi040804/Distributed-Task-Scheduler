# Distributed Task Scheduler 测试文档

## 1. 测试目标

验证系统核心模块在正常场景和异常场景下能够按照预期工作，包括：

- 协议序列化与反序列化；
- Worker 注册与心跳；
- 任务提交、调度、执行和结果回传；
- 任务失败重试；
- Worker 超时与故障处理。

---

## 2. 测试环境

- 系统：Linux / WSL Ubuntu
- 编译器：支持 C++17 的 g++
- 构建工具：CMake、Make
- 网络环境：本机回环地址 `127.0.0.1`

构建与执行：

```bash
mkdir -p build
cd build
cmake ..
make
ctest --output-on-failure
```

## 3. 测试用例

### T001：Worker 注册集成测试

**测试目标**

验证 Worker 能够通过 TCP 连接 Master，发送注册消息后被 Master 正确保存。

**前置条件**

- Master 监听端口 `8080`；
- 测试 Worker 的信息为：
  - ID：`1`
  - IP：`127.0.0.1`
  - Port：`9000`

**测试步骤**

1. 启动 Master；
2. 在后台线程运行 Master 主循环；
3. TCP Client 连接 Master；
4. 构造并发送 `REGISTER_WORKER` 消息；
5. 等待 Master 处理注册消息；
6. 从 Master 查询 Worker 信息；
7. 停止 Master，并等待主线程退出。

**预期结果**

- Master 成功接收 Worker 注册消息；
- 能够查询到 ID 为 `1` 的 Worker；
- Worker 的 ID、IP、端口与注册消息一致；
- 初始运行任务数为 `0`；
- Worker 状态为存活；
- 测试结束后 Master 线程能够正常退出。

### T002：TaskManager 单元测试

**测试目标**

验证任务管理模块能够按优先级取出任务，严格维护任务状态机，并在任务失败时按照最大重试次数重新调度或最终失败。

**前置条件**

- 创建独立的 `TaskManager` 对象；
- 不启动 Master、Worker 或网络连接；
- 最大重试次数配置为 `MAX_TASK_RETRY = 3`。

**测试步骤**

1. 创建三个优先级分别为 `1`、`10`、`5` 的任务，并依次加入 `TaskManager`；
2. 连续获取待调度任务，检查出队顺序；
3. 创建一个任务，依次执行 `PENDING → RUNNING → DONE`；
4. 尝试将已完成任务重新改为 `RUNNING`；
5. 创建一个已分配 Worker 的运行中任务，并上报 `DONE` 结果；
6. 创建一个任务，连续模拟失败；
7. 前 `3` 次失败后，检查任务是否重新进入待调度队列；
8. 第 `4` 次失败后，检查任务是否最终进入 `FAILED` 状态。

**预期结果**

- 优先级较高的任务先出队，顺序为：`10 → 5 → 1`；
- 合法状态转换 `PENDING → RUNNING → DONE` 成功；
- 非法状态转换，例如 `DONE → RUNNING`，会被拒绝；
- 任务成功后状态为 `DONE`，并清除已分配的 Worker ID；
- 任务首次执行失败后，最多允许重新调度 `3` 次；
- 超出最大重试次数后，任务状态为 `FAILED`，且不再位于待调度队列。

### T003：WorkerManager 单元测试

**测试目标**

验证 WorkerManager 能够正确维护 Worker 的注册信息、任务负载和存活状态，并为最小负载调度策略提供正确的候选节点。

**前置条件**

- 创建独立的 `WorkerManager` 对象；
- 不启动 Master、Worker 或网络连接；
- 通过 `WorkerInfo` 模拟已注册的 Worker。

**测试步骤**

1. 添加一个 Worker，并更新其运行任务数和等待任务数；
2. 尝试更新不存在的 Worker 的负载；
3. 添加多个 Worker，设置不同的总负载；
4. 设置两个 Worker 为相同最低负载，调用最小负载选择接口；
5. 将负载最低的 Worker 标记为死亡，再次执行 Worker 选择；
6. 将所有 Worker 标记为死亡，验证无可用节点时的返回值；
7. 将一个死亡 Worker 的状态通过心跳更新；
8. 创建 WorkerInfo，分别验证正常心跳时间和超时后的判断结果。

**预期结果**

- 已注册 Worker 可被查询，运行任务数、等待任务数和总负载正确；
- 更新不存在的 Worker 负载会失败；
- 调度器选择总负载最小的存活 Worker；
- 总负载相同时，选择 Worker ID 更小的节点；
- 被标记为死亡的 Worker 不参与调度；
- 所有 Worker 死亡时，选择接口返回 Worker ID `-1`；
- 死亡 Worker 收到新的心跳后恢复为存活状态；
- 心跳时间超过阈值时，Worker 被正确判定为超时。

### T004：CTest 统一测试接入

**测试目标**

将已有的测试可执行文件注册到 CTest，使项目能够通过统一命令自动执行测试并汇总结果。

**测试步骤**

1. 在 `CMakeLists.txt` 中启用 CTest；
2. 注册 `protocol_test`、`register_test`、`task_manager_test`、`worker_manager_test`；
3. 重新执行 CMake 配置与编译；
4. 执行 `ctest -N` 查看已发现的测试；
5. 执行 `ctest --output-on-failure` 运行全部测试。

**预期结果**

- CTest 能识别 4 个测试；
- 一条命令即可执行全部已注册测试；
- 所有测试均通过；
- 任一测试失败时，CTest 能输出对应失败信息。

### T005：正常任务闭环集成测试

**测试目标**

验证 Client、Master 和 Worker 能够通过真实 TCP 连接完成一次正常任务的提交、调度、执行、结果回传和最终状态更新。

**前置条件**

- Master 监听本机端口 `8081`；
- 创建 ID 为 `1` 的 Worker，并连接到 Master；
- 创建 Client，并连接到同一 Master；
- TaskExecutor 能够正常执行非空 payload 的任务。

**测试步骤**

1. 启动 Master，并在后台线程运行主循环；
2. 启动 Worker，向 Master 发送注册消息并启动心跳、任务接收和任务执行线程；
3. 轮询确认 Master 已保存该 Worker；
4. 启动 Client，提交一个优先级为 `10`、payload 为非空字符串的任务；
5. 轮询查询 Master 中 ID 为 `1` 的任务状态；
6. 在规定时间内确认任务状态变为 `DONE`；
7. 依次停止 Client、Worker、Master，并回收 Master 主线程。

**预期结果**

- Worker 能成功注册到 Master；
- Client 提交的任务被 Master 创建并加入待调度队列；
- Scheduler 能选择已注册的 Worker 并发送任务；
- Worker 能接收并执行任务，随后返回 `TASK_RESULT`；
- Master 最终将任务状态更新为 `DONE`；
- 所有线程和网络连接能够正常停止；
- CTest 中 `task_e2e_test` 通过。