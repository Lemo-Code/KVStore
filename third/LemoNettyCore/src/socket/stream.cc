#include "lemo/socket/stream.h"

#include <sys/uio.h>
#include <vector>

namespace lemo::socket {

int Stream::readFixSize(void* buffer, size_t length) {
  size_t offset = 0;
  size_t left = length;
  while (left > 0) {
    const size_t len =
        static_cast<size_t>(read(static_cast<char*>(buffer) + offset, left));
    if (len == 0) {
      return static_cast<int>(offset == 0 ? 0 : static_cast<int>(offset));
    }
    if (static_cast<int>(len) < 0) {
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
    const size_t len = static_cast<size_t>(
        write(static_cast<const char*>(buffer) + offset, left));
    if (len == 0) {
      return static_cast<int>(offset == 0 ? 0 : static_cast<int>(offset));
    }
    if (static_cast<int>(len) < 0) {
      return static_cast<int>(len);
    }
    offset += len;
    left -= len;
  }
  return static_cast<int>(length);
}

int Stream::read(buffer::RingBuffer& /*buf*/, size_t /*length*/) { return -1; }

int Stream::read(buffer::ByteArray::ptr /*ba*/, size_t /*length*/) { return -1; }

int Stream::readFixSize(buffer::RingBuffer& buf, size_t length) {
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

int Stream::readFixSize(buffer::ByteArray::ptr ba, size_t length) {
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

int Stream::write(buffer::RingBuffer& /*buf*/, size_t /*length*/) { return -1; }

int Stream::write(buffer::ByteArray::ptr /*ba*/, size_t /*length*/) { return -1; }

int Stream::writeFixSize(buffer::RingBuffer& buf, size_t length) {
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

int Stream::writeFixSize(buffer::ByteArray::ptr ba, size_t length) {
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

int SocketStream::read(buffer::RingBuffer& buf, size_t length) {
  if (!isConnected()) {
    return -1;
  }
  return static_cast<int>(buf.readFd(socket_->getSocket(), length));
}

int SocketStream::read(buffer::ByteArray::ptr ba, size_t length) {
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

int SocketStream::write(buffer::RingBuffer& buf, size_t length) {
  if (!isConnected()) {
    return -1;
  }
  return static_cast<int>(buf.writeFd(socket_->getSocket(), length));
}

int SocketStream::write(buffer::ByteArray::ptr ba, size_t length) {
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

}  // namespace lemo::socket
