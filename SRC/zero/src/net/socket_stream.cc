// zero SocketStream implementation — Stream wrapper around TCP Socket
#include "zero/net/socket_stream.h"
#include "zero/fiber/fiber.h"
#include <unistd.h>
#include <cerrno>

namespace zero {

SocketStream::SocketStream(Socket::Ptr socket)
    : socket_(std::move(socket)) {}

SocketStream::~SocketStream() {
    close();
}

ssize_t SocketStream::read(void* buf, size_t count) {
    if (!socket_ || !socket_->is_valid()) return -1;
    if (!read_buffer_.empty()) {
        return read_buffer_.read(buf, count);
    }
    return do_read(buf, count);
}

ssize_t SocketStream::readv(const struct iovec* iov, int iovcnt) {
    if (!socket_ || !socket_->is_valid()) return -1;
    if (!read_buffer_.empty()) {
        size_t total = 0;
        for (int i = 0; i < iovcnt; ++i) {
            size_t n = read_buffer_.read(iov[i].iov_base, iov[i].iov_len);
            total += n;
            if (n < iov[i].iov_len) break;
        }
        return static_cast<ssize_t>(total);
    }
    return ::readv(socket_->fd(), iov, iovcnt);
}

ssize_t SocketStream::write(const void* buf, size_t count) {
    if (!socket_ || !socket_->is_valid()) return -1;
    write_buffer_.append(buf, count);
    if (write_auto_flush_ > 0 && write_buffer_.readable_size() >= write_auto_flush_) {
        flush();
    }
    return static_cast<ssize_t>(count);
}

ssize_t SocketStream::writev(const struct iovec* iov, int iovcnt) {
    if (!socket_ || !socket_->is_valid()) return -1;
    for (int i = 0; i < iovcnt; ++i) {
        write_buffer_.append(iov[i].iov_base, iov[i].iov_len);
    }
    return 0;
}

int SocketStream::get_fd() const {
    return socket_ ? socket_->fd() : -1;
}

bool SocketStream::is_open() const {
    return socket_ && socket_->is_valid();
}

void SocketStream::close() {
    if (socket_) {
        socket_->close();
        socket_.reset();
    }
}

std::shared_ptr<Address> SocketStream::get_local_address() const {
    return socket_ ? socket_->local_address() : nullptr;
}

std::shared_ptr<Address> SocketStream::get_remote_address() const {
    return socket_ ? socket_->peer_address() : nullptr;
}

ssize_t SocketStream::flush() {
    if (!socket_ || !socket_->is_valid()) return -1;
    if (write_buffer_.empty()) return 0;
    ssize_t n = write_buffer_.write_to_fd(socket_->fd());
    if (n < 0 && errno == EAGAIN) {
        Fiber* f = Fiber::GetThis();
        if (f) f->yield();
        return 0;
    }
    return n;
}

ssize_t SocketStream::do_read(void* buf, size_t count) {
    if (!socket_ || !socket_->is_valid()) return -1;
    ssize_t n = ::recv(socket_->fd(), buf, count, 0);
    if (n < 0 && errno == EAGAIN) {
        Fiber* f = Fiber::GetThis();
        if (f) f->yield();
        return 0;
    }
    return n;
}

ssize_t SocketStream::do_write(const void* buf, size_t count) {
    if (!socket_ || !socket_->is_valid()) return -1;
    ssize_t n = ::send(socket_->fd(), buf, count, MSG_NOSIGNAL);
    if (n < 0 && errno == EAGAIN) {
        Fiber* f = Fiber::GetThis();
        if (f) f->yield();
        return 0;
    }
    return n;
}

} // namespace zero
