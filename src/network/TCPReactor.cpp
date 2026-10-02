#include <sys/epoll.h>
#include <cerrno>
#include <cstdint>
#include <sys/eventfd.h>
#include <unistd.h>
#include "network/TCPReactor.h"

namespace dts {

TCPReactor::TCPReactor(TCPServer& server)
    : server_(server),
      poller_(),
      started_(false),
      wake_fd_(-1) {
}

bool TCPReactor::start() {
    if (started_) {
        return true;
    }

    if (!poller_.valid()) {
        return false;
    }

    if (server_.listenFd() < 0) {
        return false;
    }

    if (!server_.setListenNonBlocking()) {
        return false;
    }

    if (!poller_.add(server_.listenFd())) {
        return false;
    }
    wake_fd_ = ::eventfd( 0, EFD_NONBLOCK | EFD_CLOEXEC );

    if (wake_fd_ < 0) {
        return false;
    }

    if (!poller_.add(wake_fd_)) {
        ::close(wake_fd_);
        wake_fd_ = -1;
        return false;
    }

    started_ = true;
    return true;

}

std::vector<std::pair<std::shared_ptr<Connection>, Message>>
TCPReactor::pollOnce(int timeout_ms) {
    std::vector<std::pair<std::shared_ptr<Connection>,
                    Message>> received_messages;

    if (!started_) {
        return received_messages;
    }

    const auto ready_events = poller_.wait(timeout_ms);
    for (const auto& event : ready_events) {
        const int fd = event.fd;
        if (fd == wake_fd_) {
            handleWakeup();
            continue;
        }
        // ① listening socket ready：把 accept queue drain 到 EAGAIN
        if (fd == server_.listenFd()) {

            const auto new_connections = server_.acceptAvailable();

            for (const auto& connection : new_connections) {
                if (!connection) {
                    continue;
                }

                const int client_fd = connection->fd();

                if (!poller_.add(client_fd)) {
                    server_.removeConnection(client_fd);
                }
            }

            continue;
        }

        // ② 普通 Connection ready
        auto connection = server_.getConnection(fd);

        if (!connection) {
            poller_.remove(fd);
            continue;
        }

        bool shouldRemove = false;

        // ========================================================
        // EPOLLIN：读取当前已经到达的数据
        // ========================================================
        if ((event.events & EPOLLIN) != 0) {
            const auto messages = connection->receiveAvailable();

            for (const auto& message : messages) {
                received_messages.emplace_back( connection, message );
            }

            if (connection->hasReceiveError()) {
                shouldRemove = true;
            }
        }

        // ========================================================
        // EPOLLOUT：继续发送 output buffer 中积压的数据
        // ========================================================
        if (!shouldRemove && (event.events & EPOLLOUT) != 0) {

            if (!connection->flushOutput()) {
                shouldRemove = true;
            } else if (!connection->hasPendingOutput()) {

                // 已经全部发送完成
                // 不再关注 EPOLLOUT，否则 writable socket 会造成 Reactor 空转
                if (!poller_.modify(fd, EPOLLIN)) {
                    shouldRemove = true;
                }
            }
        }

        // ========================================================
        // socket error / hangup
        // ========================================================
        if ((event.events & (EPOLLERR | EPOLLHUP)) != 0) {
            shouldRemove = true;
        }

        if (shouldRemove) {
            poller_.remove(fd);
            server_.removeConnection(fd);
        }

    }

    return received_messages;
}

bool TCPReactor::enableWrite( const std::shared_ptr<Connection>& connection) {

    if (!connection) {
        return false;
    }

    const int fd = connection->fd();

    if (fd < 0) {
        return false;
    }

    return poller_.modify( fd, EPOLLIN | EPOLLOUT );
}
bool TCPReactor::sendMessage(const std::shared_ptr<Connection>& connection, 
                                const Message& message) {

    if (!started_ || !connection) {
        return false;
    }

    const int fd = connection->fd();

    if (fd < 0) {
        return false;
    }

    if (!connection->queueMessage(message)) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock( pending_write_mutex_ );

        pending_write_fds_.insert(fd);
    }

    return wakeup();
}
bool TCPReactor::wakeup() {
    if (wake_fd_ < 0) {
        return false;
    }

    const std::uint64_t value = 1;

    while (true) {
        const ssize_t n = ::write(
            wake_fd_,
            &value,
            sizeof(value)
        );

        if (n == static_cast<ssize_t>(sizeof(value))) {
            return true;
        }

        if (n < 0 && errno == EINTR) {
            continue;
        }

        if (n < 0 &&
            (errno == EAGAIN ||
             errno == EWOULDBLOCK)) {

            // eventfd 已经处于可读状态，
            // Reactor 本来就会被唤醒。
            return true;
        }

        return false;
    }
}
void TCPReactor::handleWakeup() {
    std::uint64_t value = 0;

    while (true) {
        const ssize_t n = ::read(
            wake_fd_,
            &value,
            sizeof(value)
        );

        if (n == static_cast<ssize_t>(sizeof(value))) {
            continue;
        }

        if (n < 0 && errno == EINTR) {
            continue;
        }

        if (n < 0 &&
            (errno == EAGAIN ||
             errno == EWOULDBLOCK)) {
            break;
        }

        break;
    }

    std::unordered_set<int> pending;

    {
        std::lock_guard<std::mutex> lock( pending_write_mutex_ );

        pending.swap(pending_write_fds_);
    }

    for (const int fd : pending) {
        const auto connection = server_.getConnection(fd);

        if (!connection) {
            continue;
        }

        if (!connection->hasPendingOutput()) {
            continue;
        }

        if (!poller_.modify( fd, EPOLLIN | EPOLLOUT)) {

            poller_.remove(fd);
            server_.removeConnection(fd);
        }
    }
}
TCPReactor::~TCPReactor() {
    if (wake_fd_ >= 0) {
        poller_.remove(wake_fd_);
        ::close(wake_fd_);
        wake_fd_ = -1;
    }
}
} // namespace dts