/*
 * Connection.cpp
 * Connection 类的实现
 */
#include<sys/socket.h>
#include<unistd.h>
#include<stdexcept> // for std::runtime_error
#include<optional>// for std::nullopt
#include <limits>
#include"network/Connection.h"
#include"common/Protocol.h"
namespace dts{
    constexpr size_t MAX_MESSAGE_SIZE=1024*1024; //1MB

    Connection::Connection(int fd, sockaddr_in addr)
        : fd_(fd),
        peer_addr_(addr)
    {
    }

    //sendMessage调用序列化函数
    bool Connection::sendMessage(const Message&msg){

    std::string raw=Protocol::serialize(msg);
        //调用底层send函数
        return send(raw,raw.size());
    }

    bool Connection::send(const std::string&data,uint32_t total_len){
        if(total_len==0) return false;//通过读取length字段判断数据是否为空
        uint32_t  sent_len=0;
        
        while(sent_len<total_len){
                //从上次发送位置开始，继续发送剩余长度
                ssize_t n=::send(fd(),data.c_str()+sent_len,total_len-sent_len,0);
                if(n<=0){
                    return false;
                }else{
                    sent_len+=static_cast<size_t>(n);
                }
        }
        return true;
    }



    Message Connection::receiveMessage(){
        std::string raw=recv();
        if(raw.empty()){
            return Message{}; 
        }
        Message msg=Protocol::deserialize(raw);
        return msg;
    }

    //通用的“收满函数”
    bool Connection::recvExact(char*buffer,size_t length){
        size_t receivedLength=0;

        while(receivedLength<length){
            ssize_t n=::recv(fd(),buffer+receivedLength,length-receivedLength,0);
            if(n<=0){
                return false;
            }

            receivedLength+=static_cast<size_t>(n);
        }
        return true;
    }
    std::string Connection::recv(){
        std::string header;
        int delimiterCount=0;
        char ch='\0';

      //先读取可变长度协议头:length|type
      while(delimiterCount<2){
        if(!recvExact(&ch,1)){
            return "";
        }
        header.push_back(ch);
        if(ch=='|'){
            ++delimiterCount;
        }
      }

      const size_t firstDelimiter=header.find('|');

      if(firstDelimiter==std::string::npos||firstDelimiter==0){
        return "";
      }

      size_t dataLength=0;
      try{
        size_t parsedLength=0;
        dataLength=std::stoull(header.substr(0,firstDelimiter),&parsedLength);

        //length字段必须全部是数字
        if(parsedLength!=firstDelimiter||dataLength>MAX_MESSAGE_SIZE){
            return "";
        }
      }catch(const std::exception&e){
        return "";
      }

      std::string data(dataLength,'\0');

      if(dataLength>0&&!recvExact(data.data(),dataLength)){
        return "";
      }

      //保持recv()现有“返回完整原始协议字符串”的接口
      return header+data;

    }

    void Connection::disconnect(){
        int socketfd=fd();
        //socket文件描述符一般不为0
        if(socketfd>=0)
        {
            shutdown(socketfd,SHUT_RDWR);// 1. 先唤醒
            close(socketfd);// 2. 再释放             
            //关闭连接后将fd_置为-1；
            fd_=-1;
        }
    }
    int Connection::fd() const {
        return fd_;
    }
}
