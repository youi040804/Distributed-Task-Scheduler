#pragma once

#include <vector> 
#include <cstdint>

namespace dts {
struct EpollEvent {
    int fd;
    uint32_t events;
};

class EpollPoller {
public:
    explicit EpollPoller(int max_events = 64);
    ~EpollPoller();

    EpollPoller(const EpollPoller&) = delete;
    EpollPoller& operator=(const EpollPoller&) = delete;

    bool valid() const;

    bool add(int fd);
    bool modify(int fd, uint32_t events);
    bool remove(int fd);
    

    // 等待就绪事件，返回本轮就绪的 fd
    std::vector<EpollEvent> wait(int timeout_ms);

private:
    int epoll_fd_;
    int max_events_;
};

} // namespace dts