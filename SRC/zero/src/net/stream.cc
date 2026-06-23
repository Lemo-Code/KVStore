// zero Stream / SocketStream implementation
//
// Provides the SocketStream class: a buffered I/O stream wrapping
// a Socket. Uses ChainBuffer for both read and write buffering.
//
// Write path: data is first accumulated in the write buffer; flush()
// empties it to the socket. Small writes are batched for efficiency.
// Large writes bypass the buffer when possible (zero-copy).
//
// Read path: data is first consumed from the read buffer; when empty,
// a direct socket recv() is attempted. The read buffer can be pre-filled
// for pipelined protocols.
#include "zero/net/socket_stream.h"
#include <unistd.h>
#include <cerrno>

namespace zero {

// ============================================================
// SocketStream
// ============================================================
SocketStream::SocketStream(Socket::Ptr socket)
    : socket_(std::move(socket)) {}

SocketStream::~SocketStream() {
    close();
}

// ============================================================
// Read operations
// ============================================================
ssize_t SocketStream::read(void* buf, size_t count) {
    if (!buf || count == 0) return 0;
    if (!socket_) return -1;

    // Try the read buffer first
    if (!read_buffer_.empty()) {
        return static_cast<ssize_t>(read_buffer_.read(buf, count));
    }

    // Direct read from socket
    ssize_t n = socket_->recv(buf, count);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // In fiber context, the caller (or hook) will yield
            return n;
        }
        if (errno == EINTR) {
            return 0;  // Interrupted — caller should retry
        }
    }
    return n;
}

ssize_t SocketStream::readv(const struct iovec* iov, int iovcnt) {
    if (!iov || iovcnt <= 0) return 0;

    ssize_t total = 0;
    for (int i = 0; i < iovcnt; ++i) {
        if (iov[i].iov_len == 0) continue;

        ssize_t n = read(iov[i].iov_base, iov[i].iov_len);
        if (n < 0) {
            // If we've already read some data, return what we have
            return total > 0 ? total : n;
        }
        total += n;

        // Short read — no more data available
        if (static_cast<size_t>(n) < iov[i].iov_len) {
            break;
        }
    }
    return total;
}

// ============================================================
// Write operations
// ============================================================
ssize_t SocketStream::write(const void* buf, size_t count) {
    if (!buf || count == 0) return 0;
    if (!socket_) return -1;

    // Fast path: if write buffer is empty, try direct send
    if (write_buffer_.empty()) {
        ssize_t n = socket_->send(buf, count);
        if (n >= 0) return n;

        // Only buffer on EAGAIN — let real errors propagate
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            return n;
        }
    }

    // Slow path: buffer the data for later flush
    write_buffer_.append(buf, count);
    return static_cast<ssize_t>(count);
}

ssize_t SocketStream::writev(const struct iovec* iov, int iovcnt) {
    if (!iov || iovcnt <= 0) return 0;

    ssize_t total = 0;
    for (int i = 0; i < iovcnt; ++i) {
        if (iov[i].iov_len == 0) continue;
        ssize_t n = write(iov[i].iov_base, iov[i].iov_len);
        if (n < 0) {
            return total > 0 ? total : n;
        }
        total += n;
    }
    return total;
}

// ============================================================
// Flush
// ============================================================
ssize_t SocketStream::flush() {
    if (write_buffer_.empty()) return 0;
    if (!socket_) return -1;

    ssize_t total = 0;

    while (!write_buffer_.empty()) {
        ssize_t n = write_buffer_.write_to_fd(socket_->fd());
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Would block — stop flushing, data remains in buffer
                break;
            }
            if (errno == EINTR) continue;  // Interrupted — retry
            return total > 0 ? total : n;
        }
        if (n == 0) break;  // Socket closed
        total += n;
    }

    return total;
}

// ============================================================
// Observers
// ============================================================
int SocketStream::get_fd() const {
    return socket_ ? socket_->fd() : -1;
}

bool SocketStream::is_open() const {
    return socket_ && socket_->fd() >= 0;
}

void SocketStream::close() {
    // Flush any pending writes
    if (!write_buffer_.empty()) {
        flush();
    }
    // Close the underlying socket
    if (socket_) {
        socket_->close();
    }
}

} // namespace zero
