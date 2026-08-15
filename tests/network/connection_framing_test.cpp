#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

#include <sys/socket.h>
#include <unistd.h>

#include "common/Protocol.h"
#include "network/Connection.h"

using namespace dts;

namespace {

bool sendAll(int fd,const std::string&data){
    size_t sentLength=0;
    while(sentLength<data.size()){
        ssize_t n=::send(fd,data.data()+sentLength,data.size()-sentLength,0);

        if(n<=0){
            return false;
        }
        
        sentLength+=static_cast<size_t>(n);
    }
    return true;
}
Message makeMessage(MessageType type,const std::string&data){
    Message message;
    message.header.type=type;
    message.data=data;
    return message;
}

void testFragmentedMessageAndLargePayload(){
    std::cout << "=== Test 1: 拆包与大消息接收 ===" << std::endl;

    int fds[2];
    assert(::socketpair(AF_UNIX,SOCK_STREAM,0,fds)==0);

    sockaddr_in address{};
    Connection receiver(fds[1],address);

    const std::string payload(2048,'x');
    const Message expected=makeMessage(MessageType::SUBMIT_TASK,payload);
    const std::string raw=Protocol::serialize(expected);

    const size_t splitPosition=std::min<size_t>(5,raw.size()-1);

    std::thread sender([&](){
        assert(sendAll(fds[0],raw.substr(0,splitPosition)));

        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        assert(sendAll(fds[0],raw.substr(splitPosition)));

        ::close(fds[0]);
    });

    Message actual=receiver.receiveMessage();
    assert(actual.header.type==MessageType::SUBMIT_TASK);
    assert(actual.header.length==payload.size());
    assert(actual.data==payload);

    sender.join();
    receiver.disconnect();

    std::cout << "✅ 拆包和超过旧 999 字节上限的大消息均能正确接收"<< std::endl;
}
void testStickyPackets(){
    std::cout << "=== Test 2: 粘包顺序接收 ===" << std::endl;

    int fds[2];
    assert(::socketpair(AF_UNIX,SOCK_STREAM,0,fds)==0);

    sockaddr_in address{};
    Connection receiver(fds[1],address);

    const Message first=makeMessage(MessageType::HEARTBEAT,"first");
    const Message second=makeMessage(MessageType::TASK_RESULT,"second");
    // 故意一次性发送两条消息，模拟粘包
    assert(sendAll(fds[0],Protocol::serialize(first)+Protocol::serialize(second)));

    Message firstActual=receiver.receiveMessage();
    Message secondActual=receiver.receiveMessage();

    assert(firstActual.header.type==MessageType::HEARTBEAT);
    assert(firstActual.data=="first");

    assert(secondActual.header.type==MessageType::TASK_RESULT);
    assert(secondActual.data=="second");

    ::close(fds[0]);
    receiver.disconnect();

    std::cout << "✅ 粘在同一字节流中的两条消息可按顺序读取"<< std::endl;
}

}//namespace

int main(){
    std::cout << "========================================" << std::endl;
    std::cout << "  Connection 协议分帧测试" << std::endl;
    std::cout << "========================================" << std::endl;

    testFragmentedMessageAndLargePayload();
    testStickyPackets();

    std::cout << "========================================" << std::endl;
    std::cout << "  ✅ 所有测试通过！" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}