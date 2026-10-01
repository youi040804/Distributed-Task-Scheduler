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

bool EpollPoller::remove(int fd) {
    if (epoll_fd_ < 0 || fd < 0) {
        return false;
    }

    return ::epoll_ctl( epoll_fd_, EPOLL_CTL_DEL, fd, nullptr ) == 0;
}

std::vector<int> EpollPoller::wait(int timeout_ms) {
    std::vector<int> ready_fds;

    if (epoll_fd_ < 0) {
        return ready_fds;
    }

    std::vector<epoll_event> events(
        static_cast<std::size_t>(max_events_)
    );

    int ready_count = 0;

    do {
        
        ready_count = ::epoll_wait( epoll_fd_, events.data(), max_events_, timeout_ms );
    
    } while (ready_count < 0 && errno == EINTR);

    if (ready_count <= 0) {
        return ready_fds;
    }

    ready_fds.reserve( static_cast<std::size_t>(ready_count) );

    for (int i = 0; i < ready_count; ++i) {
        ready_fds.push_back(events[i].data.fd);
    }

    return ready_fds;
}

} // namespace dts