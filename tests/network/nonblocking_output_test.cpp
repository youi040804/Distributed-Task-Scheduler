#include <cassert>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

#include "common/Message.h"
#include "network/Connection.h"

void testQueueAndFlushOutput() {
    int fds[2];

    assert( ::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0 );

    sockaddr_in dummyAddr{};

    dts::Connection sender(fds[0], dummyAddr);
    dts::Connection receiver(fds[1], dummyAddr);

    assert(sender.setNonBlocking());

    dts::Message message;
    message.header.type = dts::MessageType::SUBMIT_TASK;
    message.data = "nonblocking-output-test";

    assert(sender.queueMessage(message));
    assert(sender.hasPendingOutput());

    assert(sender.flushOutput());
    assert(!sender.hasPendingOutput());

    const dts::Message received = receiver.receiveMessage();

    assert(received.header.type == message.header.type);
    assert(received.data == message.data);

    sender.disconnect();
    receiver.disconnect();
}

void testPendingOutputAfterSendBufferFull() {
    int fds[2];

    assert( ::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0 );

    // 尽量缩小 sender 的内核发送缓冲区，
    // 让测试更容易稳定触发 EAGAIN
    int sendBufferSize = 4096;

    assert(::setsockopt(fds[0], SOL_SOCKET, SO_SNDBUF, &sendBufferSize, sizeof(sendBufferSize)) == 0);

    sockaddr_in dummyAddr{};

    dts::Connection sender(fds[0], dummyAddr);

    assert(sender.setNonBlocking());

    // 单个协议 Message 有 1MB 上限，所以这里通过连续 queue
    // 多条消息制造远大于 socket send buffer 的待发送数据
    const std::string payload(64 * 1024, 'X');

    constexpr int messageCount = 16;

    for (int i = 0; i < messageCount; ++i) {
        dts::Message message;
        message.header.type = dts::MessageType::SUBMIT_TASK;
        message.data = payload;

        assert(sender.queueMessage(message));
    }

    assert(sender.hasPendingOutput());

    // peer 此时完全不读取
    // flush 会先写入一部分，然后在发送缓冲区满后遇到 EAGAIN
    assert(sender.flushOutput());

    // EAGAIN 不能被当成发送失败；
    // 未发送的数据必须仍然留在 output buffer
    assert(sender.hasPendingOutput());

    // 现在开始从 peer 端 drain 数据，
    // sender 再继续 flush，直到所有 output 被发送完成
    char buffer[8192];

    while (sender.hasPendingOutput()) {
        while (true) {
            const ssize_t n = ::recv(fds[1], buffer, sizeof(buffer), MSG_DONTWAIT);

            if (n > 0) {
                continue;
            }

            break;
        }

        assert(sender.flushOutput());
    }

    assert(!sender.hasPendingOutput());

    sender.disconnect();
    ::close(fds[1]);
}

int main() {
    testQueueAndFlushOutput();
    testPendingOutputAfterSendBufferFull();

    std::cout << "nonblocking_output_test passed" << std::endl;

    return 0;
}