/*
 * TCPClient.cpp
 * TCPClient 类的实现
 */
#include <arpa/inet.h> //for inet_addr()
#include <unistd.h>//for close()
#include"network/TCPClient.h"

namespace dts{
        TCPClient::TCPClient(std::string ip,int port)
        :ip_(ip),port_(port){}
        bool TCPClient::connect(){
            int client_fd=socket(AF_INET,SOCK_STREAM,0);
            if(client_fd<0){
                perror("Client socket");
                return false;
            }
            sockaddr_in server_addr={};
            server_addr.sin_family=AF_INET;

            //TCPClient要使用自己的 ip_ 和 port_
            server_addr.sin_port=htons(port_);
            //现代推荐inet_pton(),inet_pton 把结果写入第三个参数
            if (inet_pton(AF_INET, ip_.c_str(), &server_addr.sin_addr) != 1) {
                perror("Invalid IP address");
                close(client_fd);
                return false;
            }

            int ret=::connect(client_fd,(sockaddr*)&server_addr,sizeof(server_addr));
            if(ret==0){
                connection_ = std::make_unique<dts::Connection>(client_fd, server_addr);
                return true;
            }else{
                close(client_fd);
                return false;

            }
        }
        Connection* TCPClient::getConnection(){
            return connection_.get(); 
        }

}