#pragma once

#include <cstddef> 
#include <string> 
#include <vector>  

#include "common/Message.h"

namespace dts {

class FrameDecoder {
public:
    // 追加新收到的字节，并返回目前已经完整的所有消息
    std::vector<Message> feed(const std::string& data);

    // 返回当前残留的、不足以组成完整帧的字节数
    std::size_t bufferedSize() const;
    
    bool hasError() const;

private:
    std::string buffer_;
    bool error_ = false;

};

} // namespace dts