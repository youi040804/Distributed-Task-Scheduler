#include <sys/epoll.h>
#include "network/TCPReactor.h"

namespace dts {

TCPReactor::TCPReactor(TCPServer& server)
    : server_(server),
      poller_(),
      started_(false) {
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

        const auto messages = connection->receiveAvailable();

        // 先保留本轮已经成功解析出来的完整消息
        for (const auto& message : messages) {
            received_messages.emplace_back(connection, message);
        }

        // peer close / protocol error / socket error
        if (connection->hasReceiveError()) {
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
} // namespace dts