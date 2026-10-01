#include <cassert>      
#include <fcntl.h>      // fcntl, F_GETFL, O_NONBLOCK
#include <iostream>    
#include <sys/socket.h> // socketpair, AF_UNIX, SOCK_STREAM
#include <unistd.h>     // close
#include <string> 

#include "common/Message.h"
#include "common/Protocol.h"
#include "network/Connection.h"

std::string makeFrame( dts::MessageType type, const std::string& data ) {
    dts::Message message;
    message.header.type = type;
    message.data = data;

    return dts::Protocol::serialize(message);
}

void testSetNonBlocking(){
    int fds[2];

    const int result = ::socketpair(AF_UNIX, SOCK_STREAM, 0, fds);

    assert(result == 0);

    sockaddr_in dummyAddr{};

    dts::Connection connection(fds[0], dummyAddr);

    assert(connection.setNonBlocking());

    const int flags = ::fcntl(connection.fd(), F_GETFL, 0);

    assert(flags != -1);
    assert((flags & O_NONBLOCK) != 0);

    connection.disconnect();
    ::close(fds[1]);
}

void testReceiveAvailable() {
    int fds[2];

    assert( ::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0 );

    sockaddr_in dummyAddr{};
    dts::Connection connection(fds[0], dummyAddr);

    assert(connection.setNonBlocking());

    const std::string frame1 = makeFrame(dts::MessageType::SUBMIT_TASK, "first");
    const std::string frame2 = makeFrame(dts::MessageType::QUERY_TASK, "second");
    const std::string data = frame1 + frame2;

    const ssize_t sent = ::send(fds[1], data.data(), data.size(), 0);

    assert(sent == static_cast<ssize_t>(data.size()));

    const auto messages = connection.receiveAvailable();

    assert(messages.size() == 2);

    assert(messages[0].header.type == dts::MessageType::SUBMIT_TASK);
    assert(messages[0].data == "first");

    assert(messages[1].header.type == dts::MessageType::QUERY_TASK);
    assert(messages[1].data == "second");

    // receiveAvailable 最后会读到 EAGAIN，但 EAGAIN 不是连接错误
    assert(!connection.hasReceiveError());

    connection.disconnect();
    ::close(fds[1]);
}

void testPartialFrameAcrossReads() {
    int fds[2];

    assert( ::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0 );

    sockaddr_in dummyAddr{};
    dts::Connection connection(fds[0], dummyAddr);

    assert(connection.setNonBlocking());

    const std::string frame = makeFrame(dts::MessageType::SUBMIT_TASK, "hello");
    const std::size_t split = frame.size() / 2;

    // 第一次只发送前半帧
    const std::string firstHalf = frame.substr(0, split);

    const ssize_t firstSent = ::send( fds[1], firstHalf.data(), firstHalf.size(), 0 );

    assert( firstSent == static_cast<ssize_t>(firstHalf.size()) );

    // Connection 能读到数据，但 FrameDecoder 还拼不出完整 Message
    const auto firstMessages = connection.receiveAvailable();
    assert(firstMessages.empty());

    // receiveAvailable() 最终因为 EAGAIN 返回，但半包和 EAGAIN 都不是连接错误
    assert(!connection.hasReceiveError());

    // 第二次再发送剩余部分
    const std::string secondHalf = frame.substr(split);

    const ssize_t secondSent = ::send( fds[1], secondHalf.data(), secondHalf.size(), 0 );
    assert( secondSent == static_cast<ssize_t>(secondHalf.size()) );

    // 上一次残留在 FrameDecoder 中的半包应该和这次收到的数据拼成完整 Message
    const auto secondMessages = connection.receiveAvailable();

    assert(secondMessages.size() == 1);
    assert( secondMessages[0].header.type == dts::MessageType::SUBMIT_TASK );
    assert(secondMessages[0].data == "hello");

    assert(!connection.hasReceiveError());

    connection.disconnect();
    ::close(fds[1]);
}
int main() {
    testSetNonBlocking();
    testReceiveAvailable();
    testPartialFrameAcrossReads();
    
    std::cout << "nonblocking_connection_test passed" << std::endl;
    return 0;
}