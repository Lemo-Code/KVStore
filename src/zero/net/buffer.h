#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <sys/uio.h>

namespace zero {

// ============ ByteBuffer (链式块缓冲区) ============
//
// 链式内存块, 支持:
//   - 读写位置追踪
//   - scatter/gather IO (iovec)
//   - 大小端序列化
//   - 零拷贝读取路径
class ByteBuffer {
public:
    using ptr = std::shared_ptr<ByteBuffer>;

    // 内存块节点
    struct Node {
        char*  ptr;
        Node*  next;
        size_t size;

        explicit Node(size_t s);
        Node();
        ~Node();
    };

    // @param base_size 每块大小 (默认 4KB)
    explicit ByteBuffer(size_t base_size = 4096);
    ~ByteBuffer();

    // ---- 读写位置 ----
    size_t getPosition() const { return position_; }
    void   setPosition(size_t v);
    size_t getSize()      const { return size_; }
    size_t getReadSize()  const { return size_ - position_; }
    size_t getCapacity()  const { return capacity_ - position_; }

    // ---- 写入 (固定长度) ----
    void writeFInt8  (int8_t   v);
    void writeFUInt8 (uint8_t  v);
    void writeFInt16 (int16_t  v);
    void writeFUInt16(uint16_t v);
    void writeFInt32 (int32_t  v);
    void writeFUInt32(uint32_t v);
    void writeFInt64 (int64_t  v);
    void writeFUInt64(uint64_t v);

    // ---- 读取 (固定长度) ----
    int8_t   readFInt8();
    uint8_t  readFUInt8();
    int16_t  readFInt16();
    uint16_t readFUInt16();
    int32_t  readFInt32();
    uint32_t readFUInt32();
    int64_t  readFInt64();
    uint64_t readFUInt64();

    // ---- Varint ----
    void     writeInt32 (int32_t  v);
    void     writeUInt32(uint32_t v);
    void     writeInt64 (int64_t  v);
    void     writeUInt64(uint64_t v);
    int32_t  readInt32();
    uint32_t readUInt32();
    int64_t  readInt64();
    uint64_t readUInt64();

    // ---- 浮点 ----
    void   writeFloat (float v);
    void   writeDouble(double v);
    float  readFloat();
    double readDouble();

    // ---- 字符串 ----
    void        writeStringF16(const std::string& v);
    void        writeStringF32(const std::string& v);
    void        writeStringF64(const std::string& v);
    void        writeStringV64(const std::string& v);
    std::string readStringF16();
    std::string readStringF32();
    std::string readStringF64();
    std::string readStringV64();

    // ---- 原始数据 ----
    void write(const void* buf, size_t size);
    void read(void* buf, size_t size);
    void read(void* buf, size_t size, size_t position) const;

    // ---- IO vector (零拷贝) ----
    uint64_t getReadBuffers(std::vector<iovec>& buffers, uint64_t len = ~0ull) const;
    uint64_t getWriteBuffers(std::vector<iovec>& buffers, uint64_t len);

    // ---- 工具 ----
    void clear();
    bool isLittleEndian() const;
    void setIsLittleEndian(bool v);
    std::string toString() const;
    std::string toHexString() const;

    bool writeToFile(const std::string& path) const;
    bool readFromFile(const std::string& path);

    size_t getBaseSize() const { return base_size_; }

private:
    void addCapacity(size_t need);

    size_t base_size_;       // 基础块大小
    size_t position_;        // 当前读/写位置
    size_t capacity_;        // 总容量
    size_t size_;            // 数据大小
    int8_t endian_;          // 1=小端, 0=大端

    Node* root_;             // 第一块
    Node* cur_;              // 当前操作块
};

} // namespace zero
