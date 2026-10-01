#include <cassert>      
#include <iostream>     
#include <sys/socket.h> // socketpair, AF_UNIX, SOCK_STREAM
#include <unistd.h>     // close

#include "network/EpollPoller.h"

void testReadableEvent() {
    int fds[2];

    assert( ::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0 );

    dts::EpollPoller poller;

    assert(poller.valid());
    assert(poller.add(fds[0]));

    // 此时还没有任何数据，应该超时返回
    auto beforeSend = poller.wait(50);
    assert(beforeSend.empty());

    const char data = 'A';

    assert( ::send(fds[1], &data, 1, 0) == 1 );

    // fds[0] 现在可读，epoll 应该报告它
    auto ready = poller.wait(1000);

    assert(ready.size() == 1);
    assert(ready[0] == fds[0]);

    assert(poller.remove(fds[0]));

    const char secondData = 'B';

    assert( ::send(fds[1], &secondData, 1, 0) == 1 );

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