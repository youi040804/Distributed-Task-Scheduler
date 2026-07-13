#pragma oncce

#include<sys/socket.h>
#include "Connection.h"
#include<unordered_map>
#include<memory>
namespace dts{
class TCPServer{
private:
int port_;
std::unordered_map<int,std::shared_ptr<Connection>> connections_;
public:
    explicit TCPServer(int port);
    bool start();
    std::shared_ptr<Connection> acceptConnection();
    void stop();

};
}