#pragma once

#include <netinet/in.h>

#include<string>
namespace dts{
class Connection{
private:
    int fd_;
    sockaddr_in peer_addr_;
public:

    explicit Connection(int fd ,sockaddr_in addr);
    bool send(const std::string &data);
    std::string  recv();
    void disconnect();
    int fd()const;


};



}