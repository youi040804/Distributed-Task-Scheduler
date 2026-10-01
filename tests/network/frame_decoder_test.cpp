#include <cassert>  
#include <iostream> 
#include <string>   
#include <vector>   
#include "common/Message.h"
#include "common/Protocol.h"
#include "network/FrameDecoder.h"

namespace {

std::string makeFrame(dts::MessageType type, const std::string& data) {
    dts::Message message;
    message.header.type = type;
    message.data = data;

    return dts::Protocol::serialize(message);
}

// 1. 一次收到完整帧
void testCompleteFrame() {
    dts::FrameDecoder decoder;

    const std::string frame = makeFrame(dts::MessageType::SUBMIT_TASK, "hello");

    const auto messages = decoder.feed(frame);

    assert(messages.size() == 1);
    assert(messages[0].header.type == dts::MessageType::SUBMIT_TASK);
    assert(messages[0].data == "hello");
    assert(decoder.bufferedSize() == 0);
}

// 2. 第一次只收到半包
void testPartialFrame() {
    dts::FrameDecoder decoder;

    const std::string frame = makeFrame(dts::MessageType::SUBMIT_TASK, "hello");

    const std::size_t split = frame.size() / 2;

    const auto messages = decoder.feed(frame.substr(0, split));
    assert(messages.empty());
    assert(decoder.bufferedSize() == split);
}

// 3. 半包补齐后应该得到完整消息
void testPartialFrameCompleted() {
    dts::FrameDecoder decoder;

    const std::string frame = makeFrame(dts::MessageType::SUBMIT_TASK, "hello");

    const std::size_t split = frame.size() / 2;

    auto first = decoder.feed(frame.substr(0, split));

    assert(first.empty());

    auto second = decoder.feed(frame.substr(split));

    assert(second.size() == 1);
    assert(second[0].header.type == dts::MessageType::SUBMIT_TASK);
    assert(second[0].data == "hello");
    assert(decoder.bufferedSize() == 0);
}

// 4. 两个完整帧一次到达，也就是粘包
void testMultipleFrames() {
    dts::FrameDecoder decoder;

    const std::string frame1 = makeFrame(dts::MessageType::SUBMIT_TASK, "first");

    const std::string frame2 = makeFrame(dts::MessageType::QUERY_TASK, "second");

    const auto messages = decoder.feed(frame1 + frame2);

    assert(messages.size() == 2);

    assert(messages[0].header.type == dts::MessageType::SUBMIT_TASK);
    assert(messages[0].data == "first");

    assert(messages[1].header.type == dts::MessageType::QUERY_TASK);
    assert(messages[1].data == "second");

    assert(decoder.bufferedSize() == 0);
}

// 5. 第一帧完整，第二帧只到了一部分
void testCompleteFrameFollowedByPartialFrame() {
    dts::FrameDecoder decoder;

    const std::string frame1 = makeFrame(dts::MessageType::SUBMIT_TASK, "first");
    const std::string frame2 = makeFrame(dts::MessageType::QUERY_TASK, "second");

    const std::size_t split = frame2.size() / 2;

    auto first = decoder.feed(frame1 + frame2.substr(0, split));

    assert(first.size() == 1);
    assert(first[0].data == "first");

    // 第二帧剩余部分应该仍然保存在 decoder 中
    assert(decoder.bufferedSize() == split);

    auto second = decoder.feed(frame2.substr(split));

    assert(second.size() == 1);
    assert(second[0].header.type == dts::MessageType::QUERY_TASK);
    assert(second[0].data == "second");

    assert(decoder.bufferedSize() == 0);
}

// 6. 超过协议允许的最大 payload
void testOversizedFrame() {
    dts::FrameDecoder decoder;

    // FrameDecoder 当前限制为 1 MB
    // 不需要真的构造 1 MB 数据，只构造一个非法 length header
    const std::string invalid = "1048577|3|";

    const auto messages = decoder.feed(invalid);

    assert(messages.empty());
    assert(decoder.bufferedSize() == 0);
}

} // namespace

int main() {
    testCompleteFrame();
    testPartialFrame();
    testPartialFrameCompleted();
    testMultipleFrames();
    testCompleteFrameFollowedByPartialFrame();
    testOversizedFrame();

    std::cout << "frame_decoder_test passed" << std::endl;

    return 0;
}