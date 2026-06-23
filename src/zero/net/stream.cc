#include "zero/net/stream.h"
#include <cstring>

namespace zero {

ssize_t Stream::readFixed(void* buf, size_t len) {
    size_t offset = 0;
    size_t remain = len;
    char* dst = static_cast<char*>(buf);

    while (remain > 0) {
        ssize_t n = read(dst + offset, remain);
        if (n <= 0) return offset > 0 ? static_cast<ssize_t>(offset) : n;
        offset += n;
        remain -= n;
    }
    return static_cast<ssize_t>(len);
}

ssize_t Stream::writeFixed(const void* buf, size_t len) {
    size_t offset = 0;
    size_t remain = len;
    const char* src = static_cast<const char*>(buf);

    while (remain > 0) {
        ssize_t n = write(src + offset, remain);
        if (n <= 0) return offset > 0 ? static_cast<ssize_t>(offset) : n;
        offset += n;
        remain -= n;
    }
    return static_cast<ssize_t>(len);
}

ssize_t Stream::read(ByteBuffer::ptr buf, size_t len) {
    // 简化: 读入临时数据再写入 buf
    std::vector<char> tmp(len);
    ssize_t n = read(tmp.data(), len);
    if (n > 0) buf->write(tmp.data(), n);
    return n;
}

ssize_t Stream::write(ByteBuffer::ptr buf, size_t len) {
    if (len > buf->getReadSize()) len = buf->getReadSize();
    // 简化
    std::vector<char> tmp(len);
    size_t old_pos = buf->getPosition();
    buf->read(tmp.data(), len);
    ssize_t n = write(tmp.data(), len);
    if (n <= 0) buf->setPosition(old_pos);
    return n;
}

} // namespace zero
