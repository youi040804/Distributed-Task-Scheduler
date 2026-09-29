/*
 * connection_concurrent_send_test.cpp
 *
 * 验证同一个 Connection 被多个线程并发发送时
 * 每条 Message 仍然保持完整，不会发生协议帧交错
 */

#include <cassert>
#include <iostream>
#include <thread>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#include "network/Connection.h"
#include "common/Message.h"

using namespace dts;

namespace {

constexpr int MESSAGE_COUNT = 100;

void testConcurrentSend() {

    std::cout<< "=== Test: Connection 并发发送 ==="<< std::endl;

    // socketpair 创建一对已经连接好的本地 socket
    // 很适合测试 Connection 的收发逻辑
    int fds[2];

    int ret = ::socketpair(AF_UNIX,SOCK_STREAM,0,fds);

    assert(ret == 0);

    sockaddr_in dummyAddr{};

    Connection sender(fds[0], dummyAddr);
    Connection receiver(fds[1], dummyAddr);

    /*
     * 两个线程共用 sender
     *
     * thread1 模拟 Worker 心跳线程
     * thread2 模拟 Worker 任务执行线程
     */

    std::thread heartbeatThread([&sender]() {

        for(int i = 0; i < MESSAGE_COUNT; ++i) {

            Message msg;
            msg.header.type = MessageType::HEARTBEAT;
            msg.data = "heartbeat-" + std::to_string(i);

            bool ok = sender.sendMessage(msg);

            assert(ok);
        }
    });

    std::thread resultThread([&sender]() {

        for(int i = 0; i < MESSAGE_COUNT; ++i) {

            Message msg;
            msg.header.type = MessageType::TASK_RESULT;
            msg.data = "task-result-" + std::to_string(i);

            bool ok = sender.sendMessage(msg);

            assert(ok);
        }
    });

    /*
     * 两个发送线程总共发送：
     *
     * MESSAGE_COUNT 个 HEARTBEAT
     * +
     * MESSAGE_COUNT 个 TASK_RESULT
     *
     * 接收端逐条读取并验证。
     */

    int heartbeatCount = 0;
    int resultCount = 0;

    for(int i = 0;i < MESSAGE_COUNT * 2;++i){
        Message msg = receiver.receiveMessage();

        if(msg.header.type == MessageType::HEARTBEAT){
            ++heartbeatCount;
            assert(msg.data.rfind("heartbeat-",0) == 0);
        }
        else if(msg.header.type ==MessageType::TASK_RESULT){
            ++resultCount;

            assert(msg.data.rfind("task-result-",0) == 0);
        }else{
            // 如果出现 UNKNOWN 或其他类型，
            // 说明协议帧可能被破坏。
            assert(false);
        }
    }

    heartbeatThread.join();
    resultThread.join();

    assert(heartbeatCount == MESSAGE_COUNT);

    assert(resultCount == MESSAGE_COUNT
    );

    sender.disconnect();
    receiver.disconnect();

    std::cout<< " 两个线程并发发送 "<< MESSAGE_COUNT * 2<< " 条消息，所有协议帧均完整"<< std::endl;
}

}

int main() {

    std::cout<< "========================================"<< std::endl;

    std::cout<< "  Connection 并发发送测试"<< std::endl;

    std::cout<< "========================================"<< std::endl;

    testConcurrentSend();

    std::cout<< "========================================"<< std::endl;

    std::cout<< "   所有测试通过！"<< std::endl;

    std::cout<< "========================================"<< std::endl;

    return 0;
}