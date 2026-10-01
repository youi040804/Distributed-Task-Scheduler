#pragma once

#include <vector> 

namespace dts {

class EpollPoller {
public:
    explicit EpollPoller(int max_events = 64);
    ~EpollPoller();

    EpollPoller(const EpollPoller&) = delete;
    EpollPoller& operator=(const EpollPoller&) = delete;

    bool valid() const;

    bool add(int fd);
    bool remove(int fd);

    // 等待就绪事件，返回本轮就绪的 fd
    std::vector<int> wait(int timeout_ms);

private:
    int epoll_fd_;
    int max_events_;
};

} // namespace dts