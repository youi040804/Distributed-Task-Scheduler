/*
 * Connection.cpp
 * Connection 类的实现
 */
#include<sys/socket.h>
#include<unistd.h>
#include<stdexcept> // for std::runtime_error
#include"../../include/network/Connection.h"

namespace dts{

    Connection::Connection(int fd,sockaddr_in addr)
    :fd_(fd),peer_addr_(addr){
    }
    bool Connection::send(const std::string&data){
        if(data.empty())return true;//空数据直接成功
        size_t  sent_len=0;
        size_t  total_len=data.size();
        while(sent_len<total_len){
            //从上次发送位置开始，继续发送剩余长度
            ssize_t n=::send(fd(),data.c_str()+sent_len,data.size()-sent_len,0);
            if(n<=0){
                return false;
            }else{
                sent_len+=static_cast<size_t>(n);
            }
        }
        return true;
    }
    std::string Connection::recv(){
        char buffer[1000];
        ssize_t n=::recv(fd(),buffer,sizeof(buffer)-1,0);
        if(n<=0){
            return "";
        }else{
            buffer[n]='\0';
            return (std::string)buffer;
        }
    }

    void Connection::disconnect(){
        int socketfd=fd();
        //socket文件描述符一般不为0
        if(socketfd>=0)
        {
        close(socketfd);
        //关闭连接后将fd_置为-1；
        fd_=-1;
        }
    }
    int Connection::fd() const {
        return fd_;
    }
}
