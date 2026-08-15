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

### T006：任务失败重试闭环集成测试

**测试目标**

验证任务在真实 Client、Master、Worker 链路中执行失败后，能够按照最大重试次数重新调度，并在重试次数耗尽后最终失败。

**前置条件**

- Master 监听本机端口 `8082`；
- 创建 ID 为 `1` 的 Worker，并连接到 Master；
- 创建 Client，并连接到同一 Master；
- `MAX_TASK_RETRY = 3`；
- TaskExecutor 将空 payload 视为执行失败。

**测试步骤**

1. 启动 Master，并在后台线程运行主循环；
2. 启动 Worker，确认其已成功注册到 Master；
3. 启动 Client，提交一个 payload 为空的任务；
4. Worker 接收任务后执行失败，并向 Master 返回 `FAILED` 结果；
5. Master 将任务重新放回待调度队列；
6. 重复执行失败与重新调度流程；
7. 轮询查询任务状态，确认任务经过 3 次重试后最终为 `FAILED`；
8. 检查最终任务的重试次数、已分配 Worker ID，并停止所有组件。

**预期结果**

- 空 payload 的任务会被 Worker 判定为执行失败；
- 前 3 次失败后，任务重新进入 `PENDING` 并再次被调度；
- 第 4 次失败后，任务状态为 `FAILED`；
- 最终 `retry_count` 为 `4`，即首次执行加 3 次额外重试；
- 最终任务不再绑定任何 Worker，`assigned_worker` 为 `-1`；
- CTest 中 `task_retry_e2e_test` 通过。

### T007：心跳刷新与超时集成测试

**测试目标**

验证真实 Worker 能够周期性发送心跳，Master 能够刷新其存活时间；Worker 停止后，Master 能够在超时阈值后将其标记为死亡。

**前置条件**

- Master 监听本机端口 `8083`；
- 创建 ID 为 `1` 的 Worker，并连接到 Master；
- `HEARTBEAT_INTERVAL = 3` 秒；
- `HEARTBEAT_TIMEOUT = 10` 秒；
- Master 的心跳检测周期为 `3` 秒。

**测试步骤**

1. 启动 Master，并在后台线程运行主循环；
2. 启动 Worker，完成注册并启动心跳线程；
3. 查询并记录 Master 保存的初始心跳时间；
4. 等待一个心跳周期以上；
5. 再次查询 Worker 信息，确认心跳时间已更新且 Worker 仍为存活状态；
6. 停止 Worker，使其不再发送心跳；
7. 轮询等待 Master 的超时检测；
8. 确认 Worker 被标记为死亡；
9. 停止 Master 并回收主线程。

**预期结果**

- Worker 注册后处于存活状态；
- 后续真实心跳会刷新 Master 中记录的心跳时间；
- 收到新心跳后 Worker 仍处于存活状态；
- Worker 停止并超过超时阈值后，Master 将其标记为死亡；
- CTest 中 `heartbeat_timeout_e2e_test` 通过。

### T008：无可用 Worker 时任务保留集成测试

**测试目标**

验证 Master 没有任何已注册或存活 Worker 时，Scheduler 不会丢失任务，也不会将任务错误地标记为运行中。

**前置条件**

- Master 监听本机端口 `8084`；
- 不启动任何 Worker；
- 创建 Client，并连接到该 Master。

**测试步骤**

1. 启动 Master，并在后台线程运行主循环；
2. 不注册任何 Worker；
3. 启动 Client，提交一个正常的非空任务；
4. 轮询确认 Master 已创建该任务；
5. 等待多个调度周期；
6. 查询任务的状态、已分配 Worker ID 和重试次数；
7. 停止 Client、Master，并回收 Master 主线程。

**预期结果**

- Master 能成功接收 Client 提交的任务；
- Scheduler 检测到没有可用 Worker 后，会将任务放回待调度队列；
- 任务状态保持为 `PENDING`；
- 任务未绑定 Worker，`assigned_worker` 为 `-1`；
- 任务重试次数保持为 `0`；
- CTest 中 `no_worker_task_e2e_test` 通过。

### T009：任务发送失败回退单元测试

**测试目标**

验证 Scheduler 已选择存活 Worker 后，若 `TASK_ASSIGN` 消息发送失败，任务能够重新进入待调度队列，而不会被错误标记为运行中或丢失。

**前置条件**

- 创建独立的 TaskManager、WorkerManager 和 Scheduler；
- 添加一个存活 Worker；
- 为该 Worker 注入模拟发送失败的 Connection；
- 创建一个待调度任务。

**测试步骤**

1. 创建继承自 Connection 的 FailingConnection；
2. 重写 `sendMessage()`，使其始终返回 `false`；
3. 将 FailingConnection 作为 Worker 的连接加入 WorkerManager；
4. 添加一个优先级为 `10` 的待调度任务；
5. 调用一次 `schedulerOnce()`；
6. 查询任务状态、已分配 Worker ID 和重试次数；
7. 再次从待调度队列取出任务，确认该任务仍在队列中。

**预期结果**

- Scheduler 能选择该 Worker；
- 向 Worker 发送任务失败后，`schedulerOnce()` 返回 `false`；
- 任务状态保持为 `PENDING`；
- 任务未绑定 Worker，`assigned_worker` 为 `-1`；
- 任务重试次数保持为 `0`；
- 任务被重新放回待调度队列；
- CTest 中 `scheduler_failure_test` 通过。

### T010：TCP 消息分帧测试

**测试目标**

验证 Connection 能够依据自定义协议中的 `length|type|data` 长度字段完整读取一条消息，正确处理 TCP 拆包、粘包和大于固定缓冲区的消息。

**前置条件**

- 使用本地 `socketpair` 模拟双向字节流连接；
- 单条消息最大长度限制为 `1 MB`；
- 协议头格式为 `length|type|`，其中 `length` 表示业务数据长度。

**测试步骤**

1. 构造 data 长度为 `2048` 字节的消息；
2. 将同一条消息拆分为两段发送，并在两段之间短暂等待；
3. 接收端调用 `receiveMessage()`，验证是否能得到完整消息；
4. 构造两条独立消息；
5. 将两条消息拼接后一次性写入 socket，模拟粘包；
6. 接收端连续调用两次 `receiveMessage()`；
7. 分别验证两次收到的消息类型和数据内容。

**预期结果**

- 接收端不会因单次 `recv` 数据不完整而提前反序列化；
- 超过旧固定缓冲区上限的消息可以完整接收；
- 两条粘连消息可以按原始顺序被分别读取；
- 接收端只根据协议 `length` 读取当前消息的 data 部分；
- CTest 中 `connection_framing_test` 通过。