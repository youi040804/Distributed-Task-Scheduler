# Benchmark

这里记录 DTS 当前的性能测试结果，主要是给后续优化留一个可以重复测试的 baseline。

目前的 `dts_benchmark` 测的是任务端到端耗时：从开始提交第一批任务前开始计时，一直到所有任务都查询到 `DONE` 或 `FAILED` 为止。

所以这里的 throughput 是 **E2E task throughput**，里面包含任务提交、Master 调度、TCP 通信、Worker 执行、结果返回和状态查询，并不是单纯的网络吞吐。

当前 `TaskExecutor` 会模拟约 1 秒的任务执行时间。

---

## 1. Worker 线程池

之前 Worker 只有一个任务执行线程，即使 Master 连续分配多个任务，同一个 Worker 内部仍然只能串行执行。

改成可配置线程池之后，可以通过：

```bash
./dts_worker \
    --id 1 \
    --master 127.0.0.1:9001 \
    --threads 4
```

指定 Worker 的执行线程数。

为了看看这个改动到底有没有实际效果，固定：

```text
1 Master
1 Worker
20 tasks
priority = 1
```

只改变 Worker 的执行线程数。

测试结果：

| Threads | Elapsed | Throughput |
|---:|---:|---:|
| 1 | 20.3664 s | 0.9820 tasks/s |
| 2 | 10.5111 s | 1.9028 tasks/s |
| 4 | 5.4690 s | 3.6570 tasks/s |

从 1 个执行线程增加到 2 个：

```text
0.9820 → 1.9028 tasks/s
```

吞吐大约提升 `1.94×`。

从 1 个执行线程增加到 4 个：

```text
0.9820 → 3.6570 tasks/s
```

吞吐大约提升 `3.72×`。

在当前这个模拟任务下，线程数从 1 增加到 4 后，Worker 的任务处理能力基本能够随线程数一起增长。

---

## 2. 多 Worker

线程池验证完以后，又测试了一下增加 Worker 数量能不能继续提高整个系统的吞吐。

这一组固定：

```text
1 Master
30 tasks
priority = 1
2 executor threads / Worker
```

只改变 Worker 数量。

结果：

| Workers | Threads / Worker | Elapsed | Throughput |
|---:|---:|---:|---:|
| 1 | 2 | 15.2167 s | 1.9715 tasks/s |
| 2 | 2 | 9.1508 s | 3.2784 tasks/s |
| 3 | 2 | 6.2027 s | 4.8366 tasks/s |

从 1 个 Worker 增加到 2 个：

```text
1.9715 → 3.2784 tasks/s
```

吞吐大约提升 `1.66×`。

从 1 个 Worker 增加到 3 个：

```text
1.9715 → 4.8366 tasks/s
```

吞吐大约提升 `2.45×`。

对应的 30 个任务总耗时：

```text
1 Worker   15.2167 s
2 Workers   9.1508 s
3 Workers   6.2027 s
```

所以增加 Worker 后，整个 DTS 的 E2E throughput 确实继续提高，不过没有像单 Worker 内增加执行线程那样接近线性增长。

---

## 3. Least Load 分配

多 Worker 测试还顺便验证了前面修过的 Least Load 调度。

之前实际跑多 Worker 时发现过一个问题：

Master 的 Worker load 主要依赖 heartbeat 更新。如果短时间连续提交很多任务，前几个任务已经发送给 Worker，但新的 heartbeat 还没有回来，Master 看到的 load 仍然可能是 0。

结果就是：

```text
Task 1 → Worker 1 (load=0)
Task 2 → Worker 1 (load=0)
Task 3 → Worker 1 (load=0)
...
```

当时实际出现过 9 个任务全部被调度到 Worker 1 的情况。

后来在任务成功发送后增加了 Master 侧的 optimistic load accounting：

```text
任务成功发送给 Worker
        ↓
Master 本地记录的 queued load +1
        ↓
下一次调度能够立即看到这个负载变化
        ↓
后续 heartbeat 再用真实的 running + queued 重新同步
```

重新测试后，多 Worker 下的任务已经能够正常分散。

这次 `3 Workers × 2 threads × 30 tasks` 的实验开始时：

```text
Task 1  → Worker 1 (load=0)
Task 3  → Worker 2 (load=0)
Task 7  → Worker 3 (load=0)

Task 15 → Worker 1 (load=1)
Task 30 → Worker 2 (load=1)
Task 29 → Worker 3 (load=1)

Task 28 → Worker 1 (load=2)
Task 27 → Worker 2 (load=2)
Task 26 → Worker 3 (load=2)
```

30 个任务最后大约分成：

```text
Worker 1 → 11 tasks
Worker 2 → 10 tasks
Worker 3 →  9 tasks
```

这里并不是固定的 Round Robin。

Master 会根据当前记录的 Worker load 做 Least Load 选择，而 heartbeat 还会不断用 Worker 实际的 `running + queued` 状态重新校准负载，所以后面的分配不一定严格保持 `1 → 2 → 3 → 1 → 2 → 3`。

---

## 4. 怎么跑

先启动 Master：

```bash
./dts_master --port 9001
```

启动一个 Worker：

```bash
./dts_worker \
    --id 1 \
    --master 127.0.0.1:9001 \
    --threads 2
```

然后运行：

```bash
./dts_benchmark \
    --master 127.0.0.1:9001 \
    --tasks 30 \
    --priority 1
```

输出类似：

```text
[Benchmark] connected to Master
[Benchmark] tasks=30, priority=1
[Benchmark] submitted 30 tasks
[Benchmark] completed=30, failed=0
[Benchmark] elapsed=15.2167 s
[Benchmark] throughput=1.97152 tasks/s
```

测试多个 Worker 时，只需要使用不同的 Worker ID 连接同一个 Master：

```bash
./dts_worker --id 1 --master 127.0.0.1:9001 --threads 2
./dts_worker --id 2 --master 127.0.0.1:9001 --threads 2
./dts_worker --id 3 --master 127.0.0.1:9001 --threads 2
```

为了避免上一组测试留下的状态影响下一组数据，目前每换一组配置都会重新启动 Master 和 Worker。

---

## 5. 测试口径

`dts_benchmark` 的计时范围是：

```text
start
  ↓
提交所有任务
  ↓
Master 调度
  ↓
Worker 执行
  ↓
结果返回
  ↓
查询所有任务状态
  ↓
全部进入 DONE / FAILED
  ↓
end
```

使用：

```cpp
std::chrono::steady_clock
```

记录 elapsed time。

throughput 目前按照：

```text
completed_tasks / elapsed_seconds
```

计算。

Benchmark 每隔约 100 ms 查询一次还没有完成的任务，因此最终 elapsed 会带有少量轮询产生的观测误差。

对于目前一个任务约 1 秒的测试场景，这个误差暂时可以接受。

---

## 6. 关于后面的 epoll

这组数据暂时不能拿来直接说明 epoll 的性能。

原因是当前 Benchmark 测的是整个任务链路：

```text
Client
  ↓
Master
  ↓
Scheduler
  ↓
TCP
  ↓
Worker
  ↓
TaskExecutor
```

而且每个模拟任务本身就要执行约 1 秒，所以 Worker 数量和执行线程数对结果的影响很大。

当前 Master 的连接处理还是 thread-per-connection。后面改成 epoll 时，这组 E2E Benchmark 会继续保留，用相同 workload 做前后对比。

另外还会单独补一组 network benchmark，专门比较：

```text
thread-per-connection
        VS
epoll
```

到时候主要看并发连接数、消息处理吞吐、线程数量以及 CPU / 内存开销。

这样才能把“整个 DTS 的任务吞吐”和“网络模型本身的性能”区分开。