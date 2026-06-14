#ifndef LEMO_BUFFER_BYTE_ARRAY_H
#define LEMO_BUFFER_BYTE_ARRAY_H

#include "lemo/utils/noncopyable.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <sys/uio.h>
#include <vector>

namespace lemo::buffer {

/**
 * @brief 链式字节数组，面向 RPC/协议序列化与反序列化。
 *
 * 内存为固定大小节点链表（默认 4KB/节点），按需扩容；
 * 通过 position 游标读写，支持 getReadBuffers/getWriteBuffers 对接 scatter-gather IO。
 */
class ByteArray : lemo::utils::NonCopyable {
 public:
  typedef std::shared_ptr<ByteArray> ptr;

  struct Node {
    explicit Node(size_t s);
    Node();
    ~Node();

    uint8_t* ptr = nullptr;
    Node* next = nullptr;
    size_t size = 0;
  };

  explicit ByteArray(size_t base_size = 4096);
  ~ByteArray();

  void writeFint8(int8_t value);
  void writeFuint8(uint8_t value);
  void writeFint16(int16_t value);
  void writeFuint16(uint16_t value);
  void writeFint32(int32_t value);
  void writeFuint32(uint32_t value);
  void writeFint64(int64_t value);
  void writeFuint64(uint64_t value);

  void writeInt32(int32_t value);
  void writeUint32(uint32_t value);
  void writeInt64(int64_t value);
  void writeUint64(uint64_t value);

  void writeFloat(float value);
  void writeDouble(double value);

  void writeStringF16(const std::string& value);
  void writeStringF32(const std::string& value);
  void writeStringF64(const std::string& value);
  void writeStringVint(const std::string& value);
  void writeStringWithoutLength(const std::string& value);

  int8_t readFint8();
  uint8_t readFuint8();
  int16_t readFint16();
  uint16_t readFuint16();
  int32_t readFint32();
  uint32_t readFuint32();
  int64_t readFint64();
  uint64_t readFuint64();

  int32_t readInt32();
  uint32_t readUint32();
  int64_t readInt64();
  uint64_t readUint64();

  float readFloat();
  double readDouble();

  std::string readStringF16();
  std::string readStringF32();
  std::string readStringF64();
  std::string readStringVint();

  void clear();

  void write(const void* buf, size_t size);
  void read(void* buf, size_t size);
  void read(void* buf, size_t size, size_t position) const;

  size_t getPosition() const { return position_; }
  void setPosition(size_t v);

  bool writeToFile(const std::string& name) const;
  bool readFromFile(const std::string& name);

  size_t getBaseSize() const { return base_size_; }
  size_t getReadSize() const { return size_ - position_; }
  size_t getSize() const { return size_; }

  bool isLittleEndian() const;
  void setIsLittleEndian(bool val);

  std::string toString() const;
  std::string toHexString() const;

  uint64_t getReadBuffers(std::vector<iovec>& buffers,
                          uint64_t len = UINT64_MAX) const;
  uint64_t getReadBuffers(std::vector<iovec>& buffers, uint64_t len,
                          uint64_t position) const;
  uint64_t getWriteBuffers(std::vector<iovec>& buffers, uint64_t len);

 private:
  void addCapacity(size_t size);
  size_t getCapacity() const { return capacity_ - position_; }

  Node* nodeAtPosition(uint64_t position) const;

  size_t base_size_ = 0;
  size_t position_ = 0;
  size_t capacity_ = 0;
  size_t size_ = 0;
  int8_t endian_ = 0;
  Node* root_ = nullptr;
  Node* cur_ = nullptr;
};

}  // namespace lemo::buffer

#endif  // LEMO_BUFFER_BYTE_ARRAY_H
