/*
 * Connection.cpp
 * Connection 类的实现
 */
#include<sys/socket.h>
#include<unistd.h>
#include<stdexcept> // for std::runtime_error
#include<optional>// for std::nullopt
#include <limits>
#include <fcntl.h> // fcntl, F_GETFL, F_SETFL, O_NONBLOCK
#include <cerrno> // errno, EAGAIN, EWOULDBLOCK
#include <vector> 
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
        // 同一个 TCP Connection 可能被多个线程同时发送
        // 必须保证一条完整 Message 发送期间不会被另一条消息插入
        std::lock_guard<std::mutex> lock(send_mutex_);

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

    bool Connection::setNonBlocking() {
        if (fd_ < 0) {
            return false;
        }

        const int flags = ::fcntl(fd_, F_GETFL, 0);
        if (flags == -1) {
            return false;
        }

        if (::fcntl(fd_, F_SETFL, flags | O_NONBLOCK) == -1) {
            return false;
        }

        return true;
    }

    std::vector<Message> Connection::receiveAvailable() {
        std::vector<Message> messages;

        if (fd_ < 0 || receive_error_) {
            return messages;
        }

        char buffer[4096];

        while (true) {
            const ssize_t n = ::recv(fd_, buffer, sizeof(buffer), 0);

            if (n > 0) {
                // 本次收到的字节交给增量解帧器
                const std::string data( buffer, static_cast<std::size_t>(n) );

                auto decoded = frame_decoder_.feed(data);

                // 把本次解析出的所有完整 Message 汇总起来
                messages.insert( messages.end(), decoded.begin(), decoded.end() );

                // 协议格式错误
                if (frame_decoder_.hasError()) {
                    receive_error_ = true;
                    break;
                }

                // 继续 recv，直到把当前已经到达的数据读干净
                continue;
            }

            if (n == 0) {
                // 对端执行了正常关闭
                receive_error_ = true;
                break;
            }

            // n == -1
            if (errno == EINTR) {
                // 被信号中断，不代表连接有问题，重新 recv
                continue;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 非阻塞 socket 当前已经没有更多数据可读,正常结束，不是错误
                break;
            }

            receive_error_ = true;
            break;
        }

        return messages;
    }

    bool Connection::hasReceiveError() const {
        return receive_error_;
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
