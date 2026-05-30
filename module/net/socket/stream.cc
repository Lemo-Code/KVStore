#include "socket/stream.h"

#include "buffer/byte_array.h"
#include "buffer/ring_buffer.h"

#include <sys/uio.h>
#include <vector>

namespace net {

int Stream::readFixSize(void* buffer, size_t length) {
  size_t offset = 0;
  size_t left = length;
  while (left > 0) {
    size_t len = static_cast<size_t>(read(static_cast<char*>(buffer) + offset, left));
    if (len <= 0) {
      return static_cast<int>(len);
    }
    offset += len;
    left -= len;
  }
  return static_cast<int>(length);
}

int Stream::writeFixSize(const void* buffer, size_t length) {
  size_t offset = 0;
  size_t left = length;
  while (left > 0) {
    size_t len = static_cast<size_t>(
        write(static_cast<const char*>(buffer) + offset, left));
    if (len <= 0) {
      return static_cast<int>(len);
    }
    offset += len;
    left -= len;
  }
  return static_cast<int>(length);
}

int Stream::read(RingBuffer& /*buf*/, size_t /*length*/) { return -1; }

int Stream::read(ByteArray::ptr /*ba*/, size_t /*length*/) { return -1; }

int Stream::readFixSize(RingBuffer& buf, size_t length) {
  size_t left = length;
  while (left > 0) {
    const int len = read(buf, left);
    if (len <= 0) {
      return len;
    }
    left -= static_cast<size_t>(len);
  }
  return static_cast<int>(length);
}

int Stream::readFixSize(ByteArray::ptr ba, size_t length) {
  size_t left = length;
  while (left > 0) {
    const int len = read(ba, left);
    if (len <= 0) {
      return len;
    }
    left -= static_cast<size_t>(len);
  }
  return static_cast<int>(length);
}

int Stream::write(RingBuffer& /*buf*/, size_t /*length*/) { return -1; }

int Stream::write(ByteArray::ptr /*ba*/, size_t /*length*/) { return -1; }

int Stream::writeFixSize(RingBuffer& buf, size_t length) {
  size_t left = length;
  while (left > 0) {
    const int len = write(buf, left);
    if (len <= 0) {
      return len;
    }
    left -= static_cast<size_t>(len);
  }
  return static_cast<int>(length);
}

int Stream::writeFixSize(ByteArray::ptr ba, size_t length) {
  size_t left = length;
  while (left > 0) {
    const int len = write(ba, left);
    if (len <= 0) {
      return len;
    }
    left -= static_cast<size_t>(len);
  }
  return static_cast<int>(length);
}

SocketStream::SocketStream(Socket::ptr sock, bool owner)
    : socket_(std::move(sock)), owner_(owner) {}

SocketStream::~SocketStream() {
  if (owner_ && socket_) {
    socket_->close();
  }
}

bool SocketStream::isConnected() const {
  return socket_ && socket_->isConnected();
}

int SocketStream::read(void* buffer, size_t length) {
  if (!isConnected()) {
    return -1;
  }
  return socket_->recv(buffer, length);
}

int SocketStream::read(RingBuffer& buf, size_t length) {
  if (!isConnected()) {
    return -1;
  }
  const ssize_t n = buf.readFd(socket_->getSocket(), length);
  return static_cast<int>(n);
}

int SocketStream::read(ByteArray::ptr ba, size_t length) {
  if (!isConnected() || !ba) {
    return -1;
  }
  std::vector<iovec> iovs;
  ba->getWriteBuffers(iovs, length);
  if (iovs.empty()) {
    return 0;
  }
  const int rt = socket_->recv(iovs.data(), iovs.size());
  if (rt > 0) {
    ba->setPosition(ba->getPosition() + static_cast<size_t>(rt));
  }
  return rt;
}

int SocketStream::write(const void* buffer, size_t length) {
  if (!isConnected()) {
    return -1;
  }
  return socket_->send(buffer, length);
}

int SocketStream::write(RingBuffer& buf, size_t length) {
  if (!isConnected()) {
    return -1;
  }
  const ssize_t n = buf.writeFd(socket_->getSocket(), length);
  return static_cast<int>(n);
}

int SocketStream::write(ByteArray::ptr ba, size_t length) {
  if (!isConnected() || !ba) {
    return -1;
  }
  std::vector<iovec> iovs;
  ba->getReadBuffers(iovs, length);
  if (iovs.empty()) {
    return 0;
  }
  const int rt = socket_->send(iovs.data(), iovs.size());
  if (rt > 0) {
    ba->setPosition(ba->getPosition() + static_cast<size_t>(rt));
  }
  return rt;
}

void SocketStream::close() {
  if (socket_) {
    socket_->close();
  }
}

}  // namespace net
