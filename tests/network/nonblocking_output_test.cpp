#include <cassert>
#include <iostream>
#include <sys/socket.h>  // socketpair, AF_UNIX, SOCK_STREAM
#include <unistd.h>      // close

#include "common/Message.h"
#include "network/Connection.h"

void testQueueAndFlushOutput() {
    int fds[2];

    assert( ::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0 );
    sockaddr_in dummyAddr{};

    dts::Connection sender(fds[0], dummyAddr);
    dts::Connection receiver(fds[1], dummyAddr);

    // Reactor 中的 Connection 使用 non-blocking socket，
    // 因此这里也按照相同方式测试发送端
    assert(sender.setNonBlocking());

    dts::Message message;
    message.header.type = dts::MessageType::SUBMIT_TASK;
    message.data = "nonblocking-output-test";

    // 1. Message 进入 output buffer 后，应该存在待发送数据
    assert(sender.queueMessage(message));
    assert(sender.hasPendingOutput());

    // 2. 当前 socket 可写，flush 应该能够把完整 frame 发出去
    assert(sender.flushOutput());

    // 3. 全部发送完成后，output buffer 应该为空
    assert(!sender.hasPendingOutput());

    // 4. 接收端使用现有阻塞接口读取，验证收到的是一条完整且正确的协议消息
    const dts::Message received = receiver.receiveMessage();

    assert(received.header.type == message.header.type);
    assert(received.data == message.data);

    sender.disconnect();
    receiver.disconnect();
}

int main() {
    testQueueAndFlushOutput();

    std::cout << "nonblocking_output_test passed" << std::endl;

    return 0;
}