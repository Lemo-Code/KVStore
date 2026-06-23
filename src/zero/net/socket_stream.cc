#include "zero/net/socket_stream.h"

namespace zero {

SocketStream::SocketStream(Socket::ptr sock) : sock_(std::move(sock)) {}

SocketStream::~SocketStream() { close(); }

ssize_t SocketStream::read(void* buf, size_t len) {
    return sock_ ? sock_->recv(buf, len) : -1;
}

ssize_t SocketStream::write(const void* buf, size_t len) {
    return sock_ ? sock_->send(buf, len) : -1;
}

void SocketStream::close() {
    if (sock_) {
        sock_->close();
        sock_.reset();
    }
}

} // namespace zero
