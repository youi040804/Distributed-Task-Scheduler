#include "network/FrameDecoder.h"

#include <limits>    
#include <stdexcept> 

#include "common/Protocol.h"

namespace dts {

namespace {

constexpr std::size_t MAX_MESSAGE_SIZE = 1024 * 1024; // 1 MB

} // namespace

std::vector<Message> FrameDecoder::feed(const std::string& data) {
    if (error_) {
        return {};
    }
    // 本次 recv() 得到的数据先追加到之前残留的数据后面
    buffer_.append(data);

    std::vector<Message> messages;

    while (true) {
        // 1. 找 length 后面的第一个 '|'
        const std::size_t firstDelimiter = buffer_.find('|');

        if (firstDelimiter == std::string::npos) {
            // 连 length 都还没收完整
            break;
        }

        if (firstDelimiter == 0) {
            // length 不能为空
            error_ = true;
            buffer_.clear();
            break;
        }

        // 2. 找 type 后面的第二个 '|'
        const std::size_t secondDelimiter = buffer_.find('|', firstDelimiter + 1);

        if (secondDelimiter == std::string::npos) {
            // type 还没有收完整
            break;
        }

        // 3. 解析 payload 长度
        std::size_t dataLength = 0;

        try {
            std::size_t parsedLength = 0;

            dataLength = std::stoull(
                buffer_.substr(0, firstDelimiter),
                &parsedLength
            );

            // length 字段必须全部由数字组成
            if (parsedLength != firstDelimiter || dataLength > MAX_MESSAGE_SIZE) {
                error_ = true;
                buffer_.clear();
                break;
            }
        } catch (const std::exception&) {
            error_ = true;
            buffer_.clear();
            break;
        }

        // 4. 算出这一整帧应该有多少字节
        const std::size_t headerLength = secondDelimiter + 1;

        if (dataLength > std::numeric_limits<std::size_t>::max() - headerLength) {
            error_ = true;
            buffer_.clear();
            break;
        }

        const std::size_t frameLength = headerLength + dataLength;

        // 5. 当前 buffer 还不够一整帧
        if (buffer_.size() < frameLength) {
            break;
        }

        // 6. 已经得到一条完整帧
        const std::string raw = buffer_.substr(0, frameLength);

        Message message = Protocol::deserialize(raw);
        messages.push_back(std::move(message));

        // 7. 删除已经消费掉的这一帧
        buffer_.erase(0, frameLength);

        // 继续 while：
        // 如果后面还粘着第二条完整消息，就继续解析
    }

    return messages;
}

std::size_t FrameDecoder::bufferedSize() const {
    return buffer_.size();
}

bool FrameDecoder::hasError() const {
    return error_;
}

} // namespace dts