/*
 * TCPServer.cpp
 * TCPServer 类的实现
 */
#include <unistd.h>
#include<sys/socket.h>
#include<sys/select.h>
#include<iostream>
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

    std::shared_ptr<Connection> TCPServer::acceptConnection(){
        if(listen_fd_<0){
            return nullptr;
        }
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(listen_fd_,&read_fds);

        timeval timeout;
        timeout.tv_sec=0;
        timeout.tv_usec=200000;//最多等待200ms
        
        int ready=select(listen_fd_+1,&read_fds,nullptr,nullptr,&timeout);

        //超时，被stop()关闭，或发生错误时，交给Master::run()决定是否退出
        if(ready<=0){
            return nullptr;
        }

        sockaddr_in client_addr={};
        socklen_t addrlen=sizeof(client_addr);
        int client_fd=accept(listen_fd_,(sockaddr*)&client_addr,&addrlen);
        if(client_fd<0){
            // stop() 关闭监听 socket 时，accept 失败属于正常退出路径
            return nullptr;
        }
        connections_[client_fd] = std::make_shared<dts::Connection>(client_fd, client_addr);
        return connections_[client_fd];
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