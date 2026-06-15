#pragma once

#include <memory>
#include "zero/net/buffer.h"

namespace zero {

// ============ Stream (抽象流) ============
class Stream {
public:
    using ptr = std::shared_ptr<Stream>;
    virtual ~Stream() = default;

    // 核心接口
    virtual ssize_t read(void* buf, size_t len) = 0;
    virtual ssize_t write(const void* buf, size_t len) = 0;
    virtual void    close() = 0;

    // 便利方法: 读/写固定长度
    ssize_t readFixed(void* buf, size_t len);
    ssize_t writeFixed(const void* buf, size_t len);

    // ByteBuffer 支持
    virtual ssize_t read(ByteBuffer::ptr buf, size_t len);
    virtual ssize_t write(ByteBuffer::ptr buf, size_t len);
};

} // namespace zero
