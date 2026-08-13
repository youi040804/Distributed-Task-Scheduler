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
