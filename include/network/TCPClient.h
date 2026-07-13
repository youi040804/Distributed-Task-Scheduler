#pragma once
#include "Connection.h"
#include<memory>
#include<string>
namespace dts{
class TCPClient{
private:
    std::string ip_;
    int port_;
    std::unique_ptr<Connection> connection_;

public:

    explicit TCPClient(std::string ip,int port);
    bool connect();
    Connection* getConnection();
};


}