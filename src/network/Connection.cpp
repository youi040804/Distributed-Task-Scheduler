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

        if (fd_ < 0 || fatal_receive_error_ || peer_read_closed_){
            return messages;
        }

        char buffer[4096];

        while (true) {
            const ssize_t n = ::recv(
                fd_,
                buffer,
                sizeof(buffer),
                0
            );

            if (n > 0) {
                const std::string data(buffer, static_cast<std::size_t>(n));

                auto decoded = frame_decoder_.feed(data);

                messages.insert(
                    messages.end(),
                    decoded.begin(),
                    decoded.end()
                );

                if (frame_decoder_.hasError()) {
                    fatal_receive_error_ = true;
                    break;
                }

                continue;
            }

            if (n == 0) {
                // TCP EOF：
                // 对端不会再发送数据，但仍可能等待我们发送响应
                peer_read_closed_ = true;
                break;
            }

            if (errno == EINTR) {
                continue;
            }

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }

            fatal_receive_error_ = true;
            break;
        }

        return messages;
    }

  
    bool Connection::hasFatalReceiveError() const {
        return fatal_receive_error_;
    }

    bool Connection::isPeerReadClosed() const {
        return peer_read_closed_;
    }

    bool Connection::hasPendingOutput() const {
        std::lock_guard<std::mutex> lock(send_mutex_);

        return output_offset_ < output_buffer_.size();
    }

    bool Connection::queueMessage(const Message& message) {
        const std::string raw = Protocol::serialize(message);

        std::lock_guard<std::mutex> lock(send_mutex_);

        if (output_offset_ == output_buffer_.size()) {
            output_buffer_.clear();
            output_offset_ = 0;
        }

        output_buffer_.append(raw);

        return true;
    }
    bool Connection::flushOutput() {
        std::lock_guard<std::mutex> lock(send_mutex_);

        while (output_offset_ < output_buffer_.size()) {
            const char* data = output_buffer_.data() + output_offset_;

            const std::size_t remaining = output_buffer_.size() - output_offset_;

            const ssize_t n = ::send( fd(), data, remaining, MSG_NOSIGNAL );

            if (n > 0) {
                output_offset_ += static_cast<std::size_t>(n);
                continue;
            }

            if (n < 0 && errno == EINTR) {
                continue;
            }

            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {

                // 不是失败
                // 只是内核发送缓冲区暂时满了
                return true;
            }

            return false;
        }

        output_buffer_.clear();
        output_offset_ = 0;

        return true;
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
