#ifndef NET_SOCKET_STREAM_H
#define NET_SOCKET_STREAM_H

#include "socket/socket.h"
#include "buffer/byte_array.h"

#include <cstddef>
#include <memory>

namespace net {

class RingBuffer;

class Stream {
 public:
  typedef std::shared_ptr<Stream> ptr;
  virtual ~Stream() {}

  virtual int read(void* buffer, size_t length) = 0;
  virtual int read(RingBuffer& buf, size_t length);
  virtual int read(ByteArray::ptr ba, size_t length);
  virtual int readFixSize(void* buffer, size_t length);
  virtual int readFixSize(RingBuffer& buf, size_t length);
  virtual int readFixSize(ByteArray::ptr ba, size_t length);
  virtual int write(const void* buffer, size_t length) = 0;
  virtual int write(RingBuffer& buf, size_t length);
  virtual int write(ByteArray::ptr ba, size_t length);
  virtual int writeFixSize(const void* buffer, size_t length);
  virtual int writeFixSize(RingBuffer& buf, size_t length);
  virtual int writeFixSize(ByteArray::ptr ba, size_t length);
  virtual void close() = 0;
};

class SocketStream : public Stream {
 public:
  typedef std::shared_ptr<SocketStream> ptr;

  SocketStream(Socket::ptr sock, bool owner = true);
  ~SocketStream() override;

  int read(void* buffer, size_t length) override;
  int read(RingBuffer& buf, size_t length) override;
  int read(ByteArray::ptr ba, size_t length) override;
  int write(const void* buffer, size_t length) override;
  int write(RingBuffer& buf, size_t length) override;
  int write(ByteArray::ptr ba, size_t length) override;
  void close() override;

  bool isConnected() const;
  Socket::ptr getSocket() const { return socket_; }

 protected:
  Socket::ptr socket_;
  bool owner_;
};

}  // namespace net

#endif  // NET_SOCKET_STREAM_H
