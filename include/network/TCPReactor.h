#pragma once

#include <memory>   
#include <utility>  
#include <vector>   
#include <mutex>
#include <unordered_set>
#include "common/Message.h"
#include "network/EpollPoller.h"
#include "network/TCPServer.h"

namespace dts {

class TCPReactor {
public:
    explicit TCPReactor(TCPServer& server);
    ~TCPReactor();
    bool start();

    // 执行一轮 epoll_wait，返回本轮收到的完整 Message 及其来源 Connection
    std::vector<std::pair<std::shared_ptr<Connection>, Message>> pollOnce(int timeout_ms);
    bool sendMessage( const std::shared_ptr<Connection>& connection, const Message& message );

private:
    TCPServer& server_;
    EpollPoller poller_;
    bool started_;

    int wake_fd_;

    std::mutex pending_write_mutex_;
    std::unordered_set<int> pending_write_fds_;

    bool wakeup();
    void handleWakeup();
};

} // namespace dts