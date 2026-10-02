#pragma once

#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "common/Message.h"
#include "network/EpollPoller.h"
#include "network/TCPServer.h"

namespace dts {

class TCPReactor {
public:
    explicit TCPReactor(TCPServer& server);
    ~TCPReactor();

    bool start();
    std::vector<std::pair<std::shared_ptr<Connection>, Message>> pollOnce(int timeout_ms);
    bool sendMessage(const std::shared_ptr<Connection>& connection, const Message& message);

private:
    TCPServer& server_;
    EpollPoller poller_;
    bool started_;
    int wake_fd_;

    std::mutex pending_write_mutex_;
    std::vector<std::shared_ptr<Connection>> pending_write_connections_;

    bool wakeup();
    void handleWakeup();
    void removeConnection(int fd);
};

} // namespace dts