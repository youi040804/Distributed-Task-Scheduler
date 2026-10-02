#include <cassert>
#include <iostream>
#include <sys/epoll.h>   // EPOLLIN, EPOLLOUT
#include <sys/socket.h>  // socketpair, AF_UNIX, SOCK_STREAM
#include <unistd.h>      // close

#include "network/EpollPoller.h"

void testReadableEvent() {
    int fds[2];

    assert( ::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0 );

    dts::EpollPoller poller;

    assert(poller.valid());
    assert(poller.add(fds[0]));

    // ========================================================
    // 1. 初始状态：只监听 EPOLLIN，但当前没有数据
    // ========================================================
    auto beforeSend = poller.wait(50);

    assert(beforeSend.empty());

    // ========================================================
    // 2. 对端发送数据，fds[0] 应该变为可读
    // ========================================================
    const char firstData = 'A';

    assert( ::send(fds[1], &firstData, 1, 0) == 1 );

    auto readableEvents = poller.wait(1000);

    assert(readableEvents.size() == 1);
    assert(readableEvents[0].fd == fds[0]);
    assert( (readableEvents[0].events & EPOLLIN) != 0 );

    // ========================================================
    // 3. 消费掉数据，使 fds[0] 恢复为不可读状态
    // ========================================================
    char receivedData = '\0';

    assert( ::recv(fds[0], &receivedData, 1, 0) == 1 );
    assert(receivedData == firstData);

    // ========================================================
    // 4. 动态增加 EPOLLOUT
    // socket 当前发送缓冲区有空间，因此应该报告可写事件
    // ========================================================
    assert( poller.modify( fds[0], EPOLLIN | EPOLLOUT ) );

    auto writableEvents = poller.wait(1000);

    bool foundWritable = false;

    for (const auto& event : writableEvents) {
        if (event.fd == fds[0] && (event.events & EPOLLOUT) != 0) {
            foundWritable = true;
            break;
        }
    }

    assert(foundWritable);

    // ========================================================
    // 5. 取消 EPOLLOUT，只保留 EPOLLIN
    // 此时没有未读数据，因此应该超时返回空事件集合
    // ========================================================
    assert( poller.modify( fds[0], EPOLLIN ) );

    auto afterDisable = poller.wait(50);

    assert(afterDisable.empty());

    // ========================================================
    // 6. 从 epoll 中移除 fd
    // ========================================================
    assert(poller.remove(fds[0]));

    const char secondData = 'B';

    assert( ::send(fds[1], &secondData, 1, 0) == 1 );

    // 即使 fd 再次变为可读，因为已经 remove，epoll 也不应该再报告它
    auto afterRemove = poller.wait(50);

    assert(afterRemove.empty());

    ::close(fds[0]);
    ::close(fds[1]);
}

int main() {
    testReadableEvent();

    std::cout << "epoll_poller_test passed" << std::endl;
    return 0;
}