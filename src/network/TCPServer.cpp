/*
 * TCPServer.cpp
 * TCPServer 类的实现
 */
#include <unistd.h>
#include<sys/socket.h>
#include<iostream>
#include <fcntl.h> // fcntl, F_GETFL, F_SETFL, O_NONBLOCK
#include <cerrno> // errno, EINTR, EAGAIN, EWOULDBLOCK
#include"network/TCPServer.h"
namespace dts{

    TCPServer::TCPServer(int port)
        : port_(port),
        listen_fd_(-1)
    {
    }

    bool TCPServer::start(){
        int listen_fd=socket(AF_INET,SOCK_STREAM,0);
        if(listen_fd<0){
            perror("Server socket");
            return false;
        }
        int reuse = 1;

        if (::setsockopt( listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse) )<0){
            perror("Server setsockopt SO_REUSEADDR");
            close(listen_fd);
            return false;
        }
        // 服务器：绑定自己的地址
        sockaddr_in addr={};
        addr.sin_family=AF_INET;
        addr.sin_port=htons(port_); // 自己的端口
        addr.sin_addr.s_addr=htonl(INADDR_ANY);// 自己的 IP

        if(bind(listen_fd,(sockaddr*)&addr,sizeof(addr))<0){
            perror("Server bind");
            close(listen_fd);
            return false;
        }// 绑定自己的地址

        if(listen(listen_fd,10)<0){
            perror("Server listen");
            close(listen_fd);
            return false;
        }
        
        listen_fd_=listen_fd;
         std::cout << "Server started on port " << port_ << std::endl;
        return true;//全部成功才返回true
    }

   
    int TCPServer::listenFd() const {
        return listen_fd_;
    }

    bool TCPServer::setListenNonBlocking() {
        if (listen_fd_ < 0) {
            return false;
        }

        const int flags = ::fcntl( listen_fd_, F_GETFL, 0 );
        if (flags == -1) {
            return false;
        }

        if (::fcntl( listen_fd_, F_SETFL, flags | O_NONBLOCK ) == -1) {
            return false;
        }

        return true;
    }

    std::vector<std::shared_ptr<Connection>> TCPServer::acceptAvailable() {
        std::vector<std::shared_ptr<Connection>> accepted_connections;

        if (listen_fd_ < 0) {
            return accepted_connections;
        }

        while (true) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);

            const int client_fd = ::accept( listen_fd_, 
                reinterpret_cast<sockaddr*>(&client_addr), &client_len );

            if (client_fd >= 0) {
                auto connection = std::make_shared<Connection>(client_fd, client_addr);

                if (!connection->setNonBlocking()) {
                    connection->disconnect();
                    continue;
                }

                connections_[client_fd] = connection;
                accepted_connections.push_back(connection);

                continue;
            }

            // accept 被信号打断，重新尝试
            if (errno == EINTR) {
                continue;
            }

            // non-blocking listen socket 的 accept queue 已经取空
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }

            // 其他 accept 错误：结束本轮
            break;
        }

        return accepted_connections;
    }

    std::shared_ptr<Connection> TCPServer::getConnection(int fd) const {
        const auto it = connections_.find(fd);

        if (it == connections_.end()) {
            return nullptr;
        }

        return it->second;
    }

    void TCPServer::removeConnection(int fd) {
        const auto it = connections_.find(fd);

        if (it == connections_.end()) {
            return;
        }

        it->second->disconnect();
        connections_.erase(it);
    }
    
    void TCPServer::stop(){
        for(auto& pair:connections_){
            pair.second->disconnect();
        }
        if(listen_fd_>=0){
            close(listen_fd_);
            listen_fd_=-1;//标记为无效，防止重复关闭
        }
    }

}