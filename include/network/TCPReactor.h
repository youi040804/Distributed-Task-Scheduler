#pragma once

#include <memory>   
#include <utility>  
#include <vector>   

#include "common/Message.h"
#include "network/EpollPoller.h"
#include "network/TCPServer.h"

namespace dts {

class TCPReactor {
public:
    explicit TCPReactor(TCPServer& server);

    bool start();

    // 执行一轮 epoll_wait，返回本轮收到的完整 Message 及其来源 Connection
    std::vector<std::pair<std::shared_ptr<Connection>, Message>> pollOnce(int timeout_ms);
    bool enableWrite( const std::shared_ptr<Connection>& connection );
    
private:
    TCPServer& server_;
    EpollPoller poller_;
    bool started_;
};

} // namespace dts