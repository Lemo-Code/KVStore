#include "lemo/buffer/byte_array.h"

#include "lemo/utils/endian.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace lemo::buffer {

namespace {

uint32_t EncodeZigzag32(int32_t v) {
  if (v < 0) {
    return static_cast<uint32_t>(-v) * 2 - 1;
  }
  return static_cast<uint32_t>(v) * 2;
}

uint64_t EncodeZigzag64(int64_t v) {
  if (v < 0) {
    return static_cast<uint64_t>(-v) * 2 - 1;
  }
  return static_cast<uint64_t>(v) * 2;
}

int32_t DecodeZigzag32(uint32_t v) {
  return static_cast<int32_t>((v >> 1) ^ (~(v & 1) + 1));
}

int64_t DecodeZigzag64(uint64_t v) {
  return static_cast<int64_t>((v >> 1) ^ (~(v & 1) + 1));
}

}  // namespace

ByteArray::Node::Node(size_t s) : ptr(new uint8_t[s]), next(nullptr), size(s) {}

ByteArray::Node::Node() : ptr(nullptr), next(nullptr), size(0) {}

ByteArray::Node::~Node() { delete[] ptr; }

ByteArray::ByteArray(size_t base_size)
    : base_size_(base_size > 0 ? base_size : 4096),
      position_(0),
      capacity_(base_size_),
      size_(0),
      endian_(LEMO_BIG_ENDIAN),
      root_(new Node(base_size_)),
      cur_(root_) {}

ByteArray::~ByteArray() {
  Node* tmp = root_;
  while (tmp) {
    cur_ = tmp;
    tmp = tmp->next;
    delete cur_;
  }
}

bool ByteArray::isLittleEndian() const { return endian_ == LEMO_LITTLE_ENDIAN; }

void ByteArray::setIsLittleEndian(bool val) {
  endian_ = val ? LEMO_LITTLE_ENDIAN : LEMO_BIG_ENDIAN;
}

ByteArray::Node* ByteArray::nodeAtPosition(uint64_t position) const {
  Node* node = root_;
  const size_t index = static_cast<size_t>(position / base_size_);
  for (size_t i = 0; i < index && node; ++i) {
    node = node->next;
  }
  return node;
}

void ByteArray::writeFint8(int8_t value) { write(&value, sizeof(value)); }

void ByteArray::writeFuint8(uint8_t value) { write(&value, sizeof(value)); }

void ByteArray::writeFint16(int16_t value) {
  if (endian_ != LEMO_BYTE_ORDER) {
    value = lemo::utils::byteswap(value);
  }
  write(&value, sizeof(value));
}

void ByteArray::writeFuint16(uint16_t value) {
  if (endian_ != LEMO_BYTE_ORDER) {
    value = lemo::utils::byteswap(value);
  }
  write(&value, sizeof(value));
}

void ByteArray::writeFint32(int32_t value) {
  if (endian_ != LEMO_BYTE_ORDER) {
    value = lemo::utils::byteswap(value);
  }
  write(&value, sizeof(value));
}

void ByteArray::writeFuint32(uint32_t value) {
  if (endian_ != LEMO_BYTE_ORDER) {
    value = lemo::utils::byteswap(value);
  }
  write(&value, sizeof(value));
}

void ByteArray::writeFint64(int64_t value) {
  if (endian_ != LEMO_BYTE_ORDER) {
    value = lemo::utils::byteswap(value);
  }
  write(&value, sizeof(value));
}

void ByteArray::writeFuint64(uint64_t value) {
  if (endian_ != LEMO_BYTE_ORDER) {
    value = lemo::utils::byteswap(value);
  }
  write(&value, sizeof(value));
}

void ByteArray::writeInt32(int32_t value) {
  writeUint32(EncodeZigzag32(value));
}

void ByteArray::writeUint32(uint32_t value) {
  uint8_t tmp[5];
  uint8_t i = 0;
  while (value >= 0x80) {
    tmp[i++] = static_cast<uint8_t>((value & 0x7F) | 0x80);
    value >>= 7;
  }
  tmp[i++] = static_cast<uint8_t>(value);
  write(tmp, i);
}

void ByteArray::writeInt64(int64_t value) {
  writeUint64(EncodeZigzag64(value));
}

void ByteArray::writeUint64(uint64_t value) {
  uint8_t tmp[10];
  uint8_t i = 0;
  while (value >= 0x80) {
    tmp[i++] = static_cast<uint8_t>((value & 0x7F) | 0x80);
    value >>= 7;
  }
  tmp[i++] = static_cast<uint8_t>(value);
  write(tmp, i);
}

void ByteArray::writeFloat(float value) {
  uint32_t v = 0;
  std::memcpy(&v, &value, sizeof(value));
  writeFuint32(v);
}

void ByteArray::writeDouble(double value) {
  uint64_t v = 0;
  std::memcpy(&v, &value, sizeof(value));
  writeFuint64(v);
}

void ByteArray::writeStringF16(const std::string& value) {
  writeFuint16(static_cast<uint16_t>(value.size()));
  write(value.data(), value.size());
}

void ByteArray::writeStringF32(const std::string& value) {
  writeFuint32(static_cast<uint32_t>(value.size()));
  write(value.data(), value.size());
}

void ByteArray::writeStringF64(const std::string& value) {
  writeFuint64(static_cast<uint64_t>(value.size()));
  write(value.data(), value.size());
}

void ByteArray::writeStringVint(const std::string& value) {
  writeUint64(static_cast<uint64_t>(value.size()));
  write(value.data(), value.size());
}

void ByteArray::writeStringWithoutLength(const std::string& value) {
  write(value.data(), value.size());
}

int8_t ByteArray::readFint8() {
  int8_t v = 0;
  read(&v, sizeof(v));
  return v;
}

uint8_t ByteArray::readFuint8() {
  uint8_t v = 0;
  read(&v, sizeof(v));
  return v;
}

#define NET_BYTEARRAY_READ_FIXED(type) \
  type v = 0;                          \
  read(&v, sizeof(v));                 \
  if (endian_ == LEMO_BYTE_ORDER) {     \
    return v;                          \
  }                                    \
  return lemo::utils::byteswap(v)

int16_t ByteArray::readFint16() { NET_BYTEARRAY_READ_FIXED(int16_t); }

uint16_t ByteArray::readFuint16() { NET_BYTEARRAY_READ_FIXED(uint16_t); }

int32_t ByteArray::readFint32() { NET_BYTEARRAY_READ_FIXED(int32_t); }

uint32_t ByteArray::readFuint32() { NET_BYTEARRAY_READ_FIXED(uint32_t); }

int64_t ByteArray::readFint64() { NET_BYTEARRAY_READ_FIXED(int64_t); }

uint64_t ByteArray::readFuint64() { NET_BYTEARRAY_READ_FIXED(uint64_t); }

#undef NET_BYTEARRAY_READ_FIXED

int32_t ByteArray::readInt32() { return DecodeZigzag32(readUint32()); }

uint32_t ByteArray::readUint32() {
  uint32_t result = 0;
  for (int i = 0; i < 32; i += 7) {
    const uint8_t b = readFuint8();
    if (b < 0x80) {
      result |= static_cast<uint32_t>(b) << i;
      break;
    }
    result |= (static_cast<uint32_t>(b & 0x7F) << i);
  }
  return result;
}

int64_t ByteArray::readInt64() { return DecodeZigzag64(readUint64()); }

uint64_t ByteArray::readUint64() {
  uint64_t result = 0;
  for (int i = 0; i < 64; i += 7) {
    const uint8_t b = readFuint8();
    if (b < 0x80) {
      result |= static_cast<uint64_t>(b) << i;
      break;
    }
    result |= (static_cast<uint64_t>(b & 0x7F) << i);
  }
  return result;
}

float ByteArray::readFloat() {
  const uint32_t v = readFuint32();
  float value = 0;
  std::memcpy(&value, &v, sizeof(value));
  return value;
}

double ByteArray::readDouble() {
  const uint64_t v = readFuint64();
  double value = 0;
  std::memcpy(&value, &v, sizeof(value));
  return value;
}

std::string ByteArray::readStringF16() {
  const uint16_t len = readFuint16();
  std::string buff(len, '\0');
  if (len > 0) {
    read(&buff[0], len);
  }
  return buff;
}

std::string ByteArray::readStringF32() {
  const uint32_t len = readFuint32();
  std::string buff(len, '\0');
  if (len > 0) {
    read(&buff[0], len);
  }
  return buff;
}

std::string ByteArray::readStringF64() {
  const uint64_t len = readFuint64();
  std::string buff(static_cast<size_t>(len), '\0');
  if (len > 0) {
    read(&buff[0], static_cast<size_t>(len));
  }
  return buff;
}

std::string ByteArray::readStringVint() {
  const uint64_t len = readUint64();
  std::string buff(static_cast<size_t>(len), '\0');
  if (len > 0) {
    read(&buff[0], static_cast<size_t>(len));
  }
  return buff;
}

void ByteArray::clear() {
  position_ = 0;
  size_ = 0;
  capacity_ = base_size_;
  Node* tmp = root_->next;
  while (tmp) {
    cur_ = tmp;
    tmp = tmp->next;
    delete cur_;
  }
  root_->next = nullptr;
  cur_ = root_;
}

void ByteArray::write(const void* buf, size_t size) {
  if (size == 0) {
    return;
  }
  addCapacity(size);

  size_t npos = position_ % base_size_;
  size_t ncap = cur_->size - npos;
  size_t bpos = 0;

  while (size > 0) {
    if (ncap >= size) {
      std::memcpy(cur_->ptr + npos, static_cast<const char*>(buf) + bpos, size);
      if (cur_->size == npos + size) {
        cur_ = cur_->next;
      }
      position_ += size;
      bpos += size;
      size = 0;
    } else {
      std::memcpy(cur_->ptr + npos, static_cast<const char*>(buf) + bpos, ncap);
      position_ += ncap;
      bpos += ncap;
      size -= ncap;
      cur_ = cur_->next;
      ncap = cur_->size;
      npos = 0;
    }
  }

  if (position_ > size_) {
    size_ = position_;
  }
}

void ByteArray::read(void* buf, size_t size) {
  if (size > getReadSize()) {
    throw std::out_of_range("ByteArray::read: not enough data");
  }

  size_t npos = position_ % base_size_;
  size_t ncap = cur_->size - npos;
  size_t bpos = 0;

  while (size > 0) {
    if (ncap >= size) {
      std::memcpy(static_cast<char*>(buf) + bpos, cur_->ptr + npos, size);
      if (cur_->size == npos + size) {
        cur_ = cur_->next;
      }
      position_ += size;
      bpos += size;
      size = 0;
    } else {
      std::memcpy(static_cast<char*>(buf) + bpos, cur_->ptr + npos, ncap);
      position_ += ncap;
      bpos += ncap;
      size -= ncap;
      cur_ = cur_->next;
      ncap = cur_->size;
      npos = 0;
    }
  }
}

void ByteArray::read(void* buf, size_t size, size_t position) const {
  if (size > size_ - position) {
    throw std::out_of_range("ByteArray::read: not enough data");
  }

  Node* node = nodeAtPosition(position);
  size_t npos = position % base_size_;
  size_t ncap = node->size - npos;
  size_t bpos = 0;

  while (size > 0) {
    if (ncap >= size) {
      std::memcpy(static_cast<char*>(buf) + bpos, node->ptr + npos, size);
      if (node->size == npos + size) {
        node = node->next;
      }
      position += size;
      bpos += size;
      size = 0;
    } else {
      std::memcpy(static_cast<char*>(buf) + bpos, node->ptr + npos, ncap);
      position += ncap;
      bpos += ncap;
      size -= ncap;
      node = node->next;
      ncap = node->size;
      npos = 0;
    }
  }
}

void ByteArray::setPosition(size_t v) {
  if (v > capacity_) {
    throw std::out_of_range("ByteArray::setPosition: out of range");
  }
  position_ = v;
  if (position_ > size_) {
    size_ = position_;
  }
  cur_ = root_;
  size_t left = position_;
  while (left >= cur_->size && cur_->next) {
    left -= cur_->size;
    cur_ = cur_->next;
  }
  if (left == cur_->size && cur_->next) {
    cur_ = cur_->next;
  }
}

bool ByteArray::writeToFile(const std::string& name) const {
  std::ofstream ofs(name.c_str(), std::ios::trunc | std::ios::binary);
  if (!ofs) {
    return false;
  }

  int64_t read_size = static_cast<int64_t>(getReadSize());
  int64_t pos = static_cast<int64_t>(position_);
  Node* node = nodeAtPosition(static_cast<uint64_t>(position_));

  while (read_size > 0) {
    const int diff = static_cast<int>(pos % static_cast<int64_t>(base_size_));
    const int64_t len =
        (read_size > static_cast<int64_t>(base_size_)
             ? static_cast<int64_t>(base_size_)
             : read_size) -
        diff;
    ofs.write(reinterpret_cast<const char*>(node->ptr + diff), len);
    node = node->next;
    pos += len;
    read_size -= len;
  }
  return true;
}

bool ByteArray::readFromFile(const std::string& name) {
  std::ifstream ifs(name.c_str(), std::ios::binary);
  if (!ifs) {
    return false;
  }

  std::vector<char> chunk(base_size_);
  while (ifs) {
    ifs.read(chunk.data(), static_cast<std::streamsize>(base_size_));
    const std::streamsize got = ifs.gcount();
    if (got > 0) {
      write(chunk.data(), static_cast<size_t>(got));
    }
  }
  return true;
}

void ByteArray::addCapacity(size_t size) {
  if (size == 0) {
    return;
  }
  const size_t old_cap = getCapacity();
  if (old_cap >= size) {
    return;
  }

  size -= old_cap;
  const size_t count =
      static_cast<size_t>(std::ceil(static_cast<double>(size) / base_size_));

  Node* tail = root_;
  while (tail->next) {
    tail = tail->next;
  }

  Node* first = nullptr;
  for (size_t i = 0; i < count; ++i) {
    tail->next = new Node(base_size_);
    if (!first) {
      first = tail->next;
    }
    tail = tail->next;
    capacity_ += base_size_;
  }

  if (old_cap == 0) {
    cur_ = first;
  }
}

std::string ByteArray::toString() const {
  std::string str(getReadSize(), '\0');
  if (str.empty()) {
    return str;
  }
  read(&str[0], str.size(), position_);
  return str;
}

std::string ByteArray::toHexString() const {
  const std::string str = toString();
  std::stringstream ss;
  for (size_t i = 0; i < str.size(); ++i) {
    if (i > 0 && i % 32 == 0) {
      ss << '\n';
    }
    ss << std::setw(2) << std::setfill('0') << std::hex
       << static_cast<int>(static_cast<uint8_t>(str[i])) << ' ';
  }
  return ss.str();
}

uint64_t ByteArray::getReadBuffers(std::vector<iovec>& buffers,
                                   uint64_t len) const {
  return getReadBuffers(buffers, len, position_);
}

uint64_t ByteArray::getReadBuffers(std::vector<iovec>& buffers, uint64_t len,
                                   uint64_t position) const {
  const uint64_t avail = static_cast<uint64_t>(size_ - position);
  len = std::min(len, avail);
  if (len == 0) {
    return 0;
  }

  const uint64_t total = len;
  Node* node = nodeAtPosition(position);
  size_t npos = static_cast<size_t>(position % base_size_);
  size_t ncap = node->size - npos;

  while (len > 0) {
    iovec iov{};
    if (ncap >= len) {
      iov.iov_base = node->ptr + npos;
      iov.iov_len = static_cast<size_t>(len);
      len = 0;
    } else {
      iov.iov_base = node->ptr + npos;
      iov.iov_len = ncap;
      len -= ncap;
      node = node->next;
      ncap = node->size;
      npos = 0;
    }
    buffers.push_back(iov);
  }
  return total;
}

uint64_t ByteArray::getWriteBuffers(std::vector<iovec>& buffers, uint64_t len) {
  if (len == 0) {
    return 0;
  }
  addCapacity(static_cast<size_t>(len));

  const uint64_t total = len;
  size_t npos = position_ % base_size_;
  size_t ncap = cur_->size - npos;
  Node* node = cur_;

  while (len > 0) {
    iovec iov{};
    if (ncap >= len) {
      iov.iov_base = node->ptr + npos;
      iov.iov_len = static_cast<size_t>(len);
      len = 0;
    } else {
      iov.iov_base = node->ptr + npos;
      iov.iov_len = ncap;
      len -= ncap;
      node = node->next;
      ncap = node->size;
      npos = 0;
    }
    buffers.push_back(iov);
  }
  return total;
}

}  // namespace lemo::buffer
