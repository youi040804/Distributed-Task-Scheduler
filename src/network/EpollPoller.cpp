#include "network/EpollPoller.h"

#include <cerrno>       // errno, EINTR
#include <sys/epoll.h>  // epoll_create1, epoll_ctl, epoll_wait
#include <unistd.h>     // close

namespace dts {

EpollPoller::EpollPoller(int max_events)
    : epoll_fd_(-1),max_events_(max_events > 0 ? max_events : 64) {

    epoll_fd_ = ::epoll_create1(EPOLL_CLOEXEC);
}

EpollPoller::~EpollPoller() {
    if (epoll_fd_ >= 0) {
        ::close(epoll_fd_);
        epoll_fd_ = -1;
    }
}

bool EpollPoller::valid() const {
    return epoll_fd_ >= 0;
}

bool EpollPoller::add(int fd) {
    if (epoll_fd_ < 0 || fd < 0) {
        return false;
    }

    epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = fd;

    return ::epoll_ctl( epoll_fd_, EPOLL_CTL_ADD, fd, &event ) == 0;
}
bool EpollPoller::modify(int fd, uint32_t events) {
    if (epoll_fd_ < 0 || fd < 0) {
        return false;
    }

    epoll_event event{};
    event.events = events;
    event.data.fd = fd;

    return ::epoll_ctl( epoll_fd_, EPOLL_CTL_MOD, fd, &event ) == 0;
}

bool EpollPoller::remove(int fd) {
    if (epoll_fd_ < 0 || fd < 0) {
        return false;
    }

    return ::epoll_ctl( epoll_fd_, EPOLL_CTL_DEL, fd, nullptr ) == 0;
}
std::vector<EpollEvent> EpollPoller::wait(int timeout_ms){
   
    std::vector<EpollEvent> ready_events;
    if (epoll_fd_ < 0) {
        return ready_events;
    }

    std::vector<epoll_event> events(
        static_cast<std::size_t>(max_events_)
    );

    int ready_count = 0;

    do {
        
        ready_count = ::epoll_wait( epoll_fd_, events.data(), max_events_, timeout_ms );
    
    } while (ready_count < 0 && errno == EINTR);

    if (ready_count <= 0) {
        return ready_events;
    }

    ready_events.reserve(static_cast<std::size_t>(ready_count) );

    for (int i = 0; i < ready_count; ++i) {
        EpollEvent event;

        event.fd = events[i].data.fd;
        event.events = events[i].events;
        ready_events.push_back(event);
    }

    return ready_events;
}

} // namespace dts