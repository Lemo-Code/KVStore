#include "zero/net/buffer.h"
#include "zero/base/endian.h"

#include <cstring>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <sstream>
#include <iomanip>

namespace zero {

// ====================================================================
// Node
// ====================================================================
ByteBuffer::Node::Node(size_t s)
    : ptr(new char[s])
    , next(nullptr)
    , size(s) {}

ByteBuffer::Node::Node()
    : ptr(nullptr), next(nullptr), size(0) {}

ByteBuffer::Node::~Node() {
    delete[] ptr;
}

// ====================================================================
// ByteBuffer
// ====================================================================
ByteBuffer::ByteBuffer(size_t base_size)
    : base_size_(base_size)
    , position_(0)
    , capacity_(0)
    , size_(0)
    , endian_(IsLittleEndian() ? 1 : 0)
    , root_(nullptr)
    , cur_(nullptr) {}

ByteBuffer::~ByteBuffer() {
    Node* node = root_;
    while (node) {
        Node* next = node->next;
        delete node;
        node = next;
    }
}

void ByteBuffer::clear() {
    Node* node = root_;
    while (node) {
        Node* next = node->next;
        delete node;
        node = next;
    }
    root_ = cur_ = nullptr;
    position_ = size_ = capacity_ = 0;
}

void ByteBuffer::addCapacity(size_t need) {
    if (need == 0) return;

    size_t remain = capacity_ - position_;
    if (remain >= need) return;

    size_t extra = need - remain;
    while (extra > 0) {
        size_t block_size = std::max(base_size_, extra);
        auto* node = new Node(block_size);

        if (!root_) {
            root_ = cur_ = node;
        } else {
            cur_->next = node;
            cur_ = node;
        }
        capacity_ += block_size;
        extra -= std::min(extra, block_size);
    }
}

void ByteBuffer::setPosition(size_t v) {
    if (v > capacity_) {
        throw std::out_of_range("ByteBuffer::setPosition out of range");
    }
    position_ = v;
    if (position_ > size_) size_ = position_;

    // 定位 cur_ 到正确块
    cur_ = root_;
    size_t offset = 0;
    while (cur_ && offset + cur_->size <= position_) {
        offset += cur_->size;
        cur_ = cur_->next;
    }
}

// ====================================================================
// 写入原始数据
// ====================================================================
void ByteBuffer::write(const void* buf, size_t size) {
    addCapacity(size);

    const char* src = static_cast<const char*>(buf);
    size_t offset = 0;
    Node* node = cur_;
    size_t node_pos = position_;

    // 找到当前块和块内偏移
    {
        Node* n = root_;
        size_t pos = 0;
        while (n && pos + n->size <= position_) {
            pos += n->size;
            n = n->next;
        }
        node = n ? n : root_;
        node_pos = n ? (position_ - pos) : 0;
    }

    while (offset < size && node) {
        size_t len = std::min(size - offset, node->size - node_pos);
        memcpy(node->ptr + node_pos, src + offset, len);
        offset += len;
        node_pos = 0;
        node = node->next;
    }

    position_ += size;
    if (position_ > size_) size_ = position_;
}

// ====================================================================
// 读取原始数据
// ====================================================================
void ByteBuffer::read(void* buf, size_t size) {
    if (getReadSize() < size) {
        throw std::out_of_range("ByteBuffer::read not enough data");
    }

    char* dst = static_cast<char*>(buf);
    size_t offset = 0;
    size_t pos = position_;
    Node* node = root_;

    // 定位到 position_
    while (node && pos >= node->size) {
        pos -= node->size;
        node = node->next;
    }

    while (offset < size && node) {
        size_t len = std::min(size - offset, node->size - pos);
        memcpy(dst + offset, node->ptr + pos, len);
        offset += len;
        pos = 0;
        node = node->next;
    }

    position_ += size;
}

void ByteBuffer::read(void* buf, size_t size, size_t position) const {
    ByteBuffer* self = const_cast<ByteBuffer*>(this);
    size_t old_pos = position_;
    self->setPosition(position);
    self->read(buf, size);
    self->position_ = old_pos;
}

// ====================================================================
// 固定长度读写
// ====================================================================
#define DEF_WRITE_FIXED(T, name) \
    void ByteBuffer::name(T v) { \
        if (!endian_) v = ByteSwap(v); \
        write(&v, sizeof(v)); \
    }

DEF_WRITE_FIXED(int8_t,   writeFInt8)
DEF_WRITE_FIXED(uint8_t,  writeFUInt8)
DEF_WRITE_FIXED(int16_t,  writeFInt16)
DEF_WRITE_FIXED(uint16_t, writeFUInt16)
DEF_WRITE_FIXED(int32_t,  writeFInt32)
DEF_WRITE_FIXED(uint32_t, writeFUInt32)
DEF_WRITE_FIXED(int64_t,  writeFInt64)
DEF_WRITE_FIXED(uint64_t, writeFUInt64)

#undef DEF_WRITE_FIXED

#define DEF_READ_FIXED(T, name) \
    T ByteBuffer::name() { \
        T v; \
        read(&v, sizeof(v)); \
        if (!endian_) v = ByteSwap(v); \
        return v; \
    }

DEF_READ_FIXED(int8_t,   readFInt8)
DEF_READ_FIXED(uint8_t,  readFUInt8)
DEF_READ_FIXED(int16_t,  readFInt16)
DEF_READ_FIXED(uint16_t, readFUInt16)
DEF_READ_FIXED(int32_t,  readFInt32)
DEF_READ_FIXED(uint32_t, readFUInt32)
DEF_READ_FIXED(int64_t,  readFInt64)
DEF_READ_FIXED(uint64_t, readFUInt64)

#undef DEF_READ_FIXED

// ====================================================================
// Varint
// ====================================================================
static void writeVarint64(ByteBuffer* buf, uint64_t value) {
    while (value >= 0x80) {
        buf->writeFUInt8(static_cast<uint8_t>(value | 0x80));
        value >>= 7;
    }
    buf->writeFUInt8(static_cast<uint8_t>(value));
}

static uint64_t readVarint64(ByteBuffer* buf) {
    uint64_t result = 0;
    int shift = 0;
    while (true) {
        uint8_t b = buf->readFUInt8();
        result |= static_cast<uint64_t>(b & 0x7F) << shift;
        if (!(b & 0x80)) break;
        shift += 7;
    }
    return result;
}

void ByteBuffer::writeInt32(int32_t v)  { writeVarint64(this, static_cast<uint32_t>((v << 1) ^ (v >> 31))); }
void ByteBuffer::writeUInt32(uint32_t v) { writeVarint64(this, v); }
void ByteBuffer::writeInt64(int64_t v)  { writeVarint64(this, static_cast<uint64_t>((v << 1) ^ (v >> 63))); }
void ByteBuffer::writeUInt64(uint64_t v) { writeVarint64(this, v); }

int32_t  ByteBuffer::readInt32()  { uint32_t v = readVarint64(this); return (v >> 1) ^ -(int32_t)(v & 1); }
uint32_t ByteBuffer::readUInt32() { return static_cast<uint32_t>(readVarint64(this)); }
int64_t  ByteBuffer::readInt64()  { uint64_t v = readVarint64(this); return (v >> 1) ^ -(int64_t)(v & 1); }
uint64_t ByteBuffer::readUInt64() { return readVarint64(this); }

// ====================================================================
// 浮点
// ====================================================================
void ByteBuffer::writeFloat(float v)   { writeFInt32(*reinterpret_cast<int32_t*>(&v)); }
void ByteBuffer::writeDouble(double v) { writeFInt64(*reinterpret_cast<int64_t*>(&v)); }
float  ByteBuffer::readFloat()         { int32_t v = readFInt32(); return *reinterpret_cast<float*>(&v); }
double ByteBuffer::readDouble()        { int64_t v = readFInt64(); return *reinterpret_cast<double*>(&v); }

// ====================================================================
// 字符串
// ====================================================================
void ByteBuffer::writeStringF16(const std::string& v) { writeFUInt16(v.size()); write(v.data(), v.size()); }
void ByteBuffer::writeStringF32(const std::string& v) { writeFUInt32(v.size()); write(v.data(), v.size()); }
void ByteBuffer::writeStringF64(const std::string& v) { writeFUInt64(v.size()); write(v.data(), v.size()); }
void ByteBuffer::writeStringV64(const std::string& v) { writeUInt64(v.size()); write(v.data(), v.size()); }

std::string ByteBuffer::readStringF16() { uint16_t len = readFUInt16(); std::string s(len, 0); read(&s[0], len); return s; }
std::string ByteBuffer::readStringF32() { uint32_t len = readFUInt32(); std::string s(len, 0); read(&s[0], len); return s; }
std::string ByteBuffer::readStringF64() { uint64_t len = readFUInt64(); std::string s(len, 0); read(&s[0], len); return s; }
std::string ByteBuffer::readStringV64() { uint64_t len = readUInt64();  std::string s(len, 0); read(&s[0], len); return s; }

// ====================================================================
// IO Vector (零拷贝)
// ====================================================================
uint64_t ByteBuffer::getReadBuffers(std::vector<iovec>& buffers, uint64_t len) const {
    if (len == 0 || len > (size_ - position_)) len = size_ - position_;
    if (len == 0) return 0;

    Node* node = root_;
    size_t pos = position_;

    // 定位
    while (node && pos >= node->size) {
        pos -= node->size;
        node = node->next;
    }

    uint64_t total = 0;
    while (node && total < len) {
        size_t chunk = std::min(static_cast<size_t>(len - total), node->size - pos);
        iovec iov;
        iov.iov_base = node->ptr + pos;
        iov.iov_len = chunk;
        buffers.push_back(iov);
        total += chunk;
        pos = 0;
        node = node->next;
    }
    return total;
}

uint64_t ByteBuffer::getWriteBuffers(std::vector<iovec>& buffers, uint64_t len) {
    addCapacity(len);

    size_t offset = 0;
    Node* node = root_;
    size_t pos = position_;

    // 定位
    while (node && pos >= node->size) {
        pos -= node->size;
        node = node->next;
    }

    uint64_t total = 0;
    while (node && total < len) {
        size_t chunk = std::min(static_cast<size_t>(len - total), node->size - pos);
        iovec iov;
        iov.iov_base = node->ptr + pos;
        iov.iov_len = chunk;
        buffers.push_back(iov);
        total += chunk;
        pos = 0;
        node = node->next;
    }

    position_ += total;
    if (position_ > size_) size_ = position_;
    return total;
}

// ====================================================================
// 工具
// ====================================================================
bool ByteBuffer::isLittleEndian() const { return endian_ == 1; }
void ByteBuffer::setIsLittleEndian(bool v) { endian_ = v ? 1 : 0; }

std::string ByteBuffer::toString() const {
    return std::string(static_cast<const char*>(nullptr), 0);
    // 简化实现
}

std::string ByteBuffer::toHexString() const {
    std::stringstream ss;
    Node* node = root_;
    size_t count = 0;

    while (node && count < size_) {
        for (size_t j = 0; j < node->size && count < size_; ++j, ++count) {
            if (count > 0 && count % 32 == 0) ss << "\n";
            else if (count > 0) ss << " ";
            ss << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<int>(static_cast<uint8_t>(node->ptr[j]));
        }
        node = node->next;
    }
    return ss.str();
}

bool ByteBuffer::writeToFile(const std::string& path) const {
    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) return false;

    size_t old_pos = position_;
    const_cast<ByteBuffer*>(this)->setPosition(0);

    Node* node = root_;
    size_t remaining = size_;
    while (node && remaining > 0) {
        size_t chunk = std::min(node->size, remaining);
        ofs.write(node->ptr, chunk);
        remaining -= chunk;
        node = node->next;
    }

    const_cast<ByteBuffer*>(this)->position_ = old_pos;
    return ofs.good();
}

bool ByteBuffer::readFromFile(const std::string& path) {
    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    if (!ifs) return false;

    size_t file_size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    clear();
    addCapacity(file_size);
    size_ = file_size;
    setPosition(0);

    Node* node = root_;
    size_t remaining = file_size;
    while (node && remaining > 0) {
        size_t chunk = std::min(node->size, remaining);
        ifs.read(node->ptr, chunk);
        remaining -= chunk;
        node = node->next;
    }

    return ifs.good() || ifs.eof();
}

} // namespace zero
