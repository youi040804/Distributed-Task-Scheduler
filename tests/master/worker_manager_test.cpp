#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

#include "common/WorkerInfo.h"
#include "master/WorkerManager.h"

using namespace dts;

void testWorkerRegistrationAndLoadUpdate() {
    std::cout << "=== Test 1: Worker 注册与负载更新 ===" << std::endl;
    WorkerManager manager;
    manager.addWorker(WorkerInfo(1,"127.0.0.1",9001),nullptr);

    assert(manager.hasWorker(1));
    assert(!manager.hasWorker(999));

    assert(manager.updateWorkerLoad(1,2,3));
    assert(!manager.updateWorkerLoad(999,1,1));

    auto worker=manager.getWorkerInfo(1);
    assert(worker.has_value());
    assert(worker->getRunningTaskCount()==2);
    assert(worker->getQueuedTaskCount()==3);
    assert(worker->getWorkerLoad()==5);

    std::cout << "✅ Worker 信息与负载更新正确" << std::endl;
}

void testLeastLoadScheduling() {
    std::cout << "=== Test 2: 最小负载调度 ===" << std::endl;
    WorkerManager manager;

    manager.addWorker(WorkerInfo(1, "127.0.0.1", 9001), nullptr);
    manager.addWorker(WorkerInfo(2, "127.0.0.1", 9002), nullptr);
    manager.addWorker(WorkerInfo(3, "127.0.0.1", 9003), nullptr);
    
    // Worker 1: 运行 1 个 + 排队 2 个，总负载 3
    assert(manager.updateWorkerLoad(1,1,2));
    // Worker 2: 运行 0 个 + 排队 1 个，总负载 1
    assert(manager.updateWorkerLoad(2,0,1));

    // Worker 3: 运行 1 个 + 排队 0 个，总负载 1
    assert(manager.updateWorkerLoad(3,1,0));

    auto[workerId,load]=manager.pickLeastLoadedWorker();
    // 2 和 3 负载相同，选择 ID 更小的 2
    assert(workerId==2);
    assert(load==1);

    std::cout << "✅ 选择负载最低 Worker；负载相同时选择 ID 更小者"
              << std::endl;
}

void testDeadWorkerIsExcluded() {
    std::cout << "=== Test 3: 死亡 Worker 不参与调度 ===" << std::endl;

   
    WorkerManager manager;

    manager.addWorker(WorkerInfo(1, "127.0.0.1", 9001), nullptr);
    manager.addWorker(WorkerInfo(2, "127.0.0.1", 9002), nullptr);

    assert(manager.updateWorkerLoad(1,0,3));
    assert(manager.updateWorkerLoad(2,0,0));

    // Worker 2 虽然负载最低，但死亡后不能被调度
    assert(manager.markWorkerDead(2));
    auto[workerId,load]=manager.pickLeastLoadedWorker();
    assert(workerId==1);
    assert(load==3);
    // 所有 Worker 都死亡时，没有可调度节点
    assert(manager.markWorkerDead(1));

    std::tie(workerId,load)=manager.pickLeastLoadedWorker();
    assert(workerId==-1);
    assert(load==0);
    std::cout << "✅ 死亡 Worker 已从调度候选集合排除" << std::endl;
}

void testHeartbeatRestoresAliveState() {
    std::cout << "=== Test 4: 心跳更新存活状态 ===" << std::endl;

    WorkerManager manager;
    manager.addWorker(WorkerInfo(1, "127.0.0.1", 9001), nullptr);

    assert(manager.markWorkerDead(1));
    auto worker=manager.getWorkerInfo(1);
    assert(worker.has_value());
    assert(!worker->isAlive());

    // 新心跳到达后，Worker 恢复为存活状态
    assert(manager.updateWorkerHeartbeat(1));

    worker=manager.getWorkerInfo(1);
    assert(worker.has_value());
    assert(worker->isAlive());

    assert(!manager.updateWorkerHeartbeat(999));

    std::cout << "✅ 收到心跳后 Worker 恢复为存活状态" << std::endl;
}

void testWorkerInfoTimeout() {
    std::cout << "=== Test 5: Worker 心跳超时判定 ===" << std::endl;

    WorkerInfo worker(1,"127.0.0.1",9001);


    // 用 0 秒阈值快速验证“时间流逝后会超时”的判断逻辑
    std::this_thread::sleep_for(std::chrono::microseconds(10));
    assert(worker.isOverTime(0));

    worker.updateHeartbeat();
    assert(!worker.isOverTime(1));
    std::cout << "✅ 心跳超时判断正确" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  WorkerManager 单元测试" << std::endl;
    std::cout << "========================================" << std::endl;

    testWorkerRegistrationAndLoadUpdate();
    testLeastLoadScheduling();
    testDeadWorkerIsExcluded();
    testHeartbeatRestoresAliveState();
    testWorkerInfoTimeout();

    std::cout << "========================================" << std::endl;
    std::cout << "  ✅ 所有测试通过！" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}