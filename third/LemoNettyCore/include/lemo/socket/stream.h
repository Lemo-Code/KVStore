#pragma once

#include "lemo/buffer/byte_array.h"
#include "lemo/buffer/ring_buffer.h"
#include "lemo/socket/socket.h"

#include <cstddef>
#include <memory>

namespace lemo::socket {

class Stream {
 public:
  typedef std::shared_ptr<Stream> ptr;
  virtual ~Stream() {}

  virtual int read(void* buffer, size_t length) = 0;
  virtual int read(buffer::RingBuffer& buf, size_t length);
  virtual int read(buffer::ByteArray::ptr ba, size_t length);
  virtual int readFixSize(void* buffer, size_t length);
  virtual int readFixSize(buffer::RingBuffer& buf, size_t length);
  virtual int readFixSize(buffer::ByteArray::ptr ba, size_t length);
  virtual int write(const void* buffer, size_t length) = 0;
  virtual int write(buffer::RingBuffer& buf, size_t length);
  virtual int write(buffer::ByteArray::ptr ba, size_t length);
  virtual int writeFixSize(const void* buffer, size_t length);
  virtual int writeFixSize(buffer::RingBuffer& buf, size_t length);
  virtual int writeFixSize(buffer::ByteArray::ptr ba, size_t length);
  virtual void close() = 0;
};

class SocketStream : public Stream {
 public:
  typedef std::shared_ptr<SocketStream> ptr;

  SocketStream(Socket::ptr sock, bool owner = true);
  ~SocketStream() override;

  int read(void* buffer, size_t length) override;
  int read(buffer::RingBuffer& buf, size_t length) override;
  int read(buffer::ByteArray::ptr ba, size_t length) override;
  int write(const void* buffer, size_t length) override;
  int write(buffer::RingBuffer& buf, size_t length) override;
  int write(buffer::ByteArray::ptr ba, size_t length) override;
  void close() override;

  bool isConnected() const;
  Socket::ptr getSocket() const { return socket_; }

 protected:
  Socket::ptr socket_;
  bool owner_;
};

}  // namespace lemo::socket
