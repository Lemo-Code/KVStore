// zero SocketStream — Stream over a Socket with buffer management
//
// The main concrete implementation of Stream for TCP socket connections.
// Wraps a Socket::Ptr and provides fiber-aware I/O.
//
// Features:
//   - Dual buffers: read buffer (data received from socket) and write
//     buffer (data to be sent). This enables zero-copy I/O: read directly
//     into the read buffer, write directly from the write buffer.
//   - Automatic buffering: write() appends to the write buffer; flush()
//     sends it to the socket.
//   - Scatter/gather: readv() uses read buffer blocks as iovec targets;
//     writev() uses them as iovec sources.
//   - Fiber-aware: on EAGAIN, yields the fiber and resumes when the
//     reactor signals the fd is ready.
//
// Usage:
//   auto sock = Socket::create_tcp();
//   sock->connect(addr);
//   auto stream = std::make_shared<SocketStream>(sock);
//   stream->write("GET / HTTP/1.1\r\n\r\n");
//   stream->flush();
//   std::string response = stream->read_all();
#pragma once

#include "zero/net/stream.h"
#include "zero/net/socket.h"
#include "zero/net/buffer.h"

namespace zero {

class SocketStream : public Stream {
public:
    using Ptr = std::shared_ptr<SocketStream>;

    // ============================================================
    // Construction
    // ============================================================

    // Create a SocketStream from an existing Socket.
    // The socket must be connected (for TCP).
    explicit SocketStream(Socket::Ptr socket);
    ~SocketStream() override;

    // ============================================================
    // Stream interface
    // ============================================================

    // Read from the socket into buf. First drains the read buffer
    // if it has data; otherwise reads directly from the socket.
    ssize_t read(void* buf, size_t count) override;

    // Scatter read: fill multiple buffers from the read buffer
    // and/or directly from the socket.
    ssize_t readv(const struct iovec* iov, int iovcnt) override;

    // Write to the write buffer (not directly to the socket).
    // Call flush() to send buffered data to the socket.
    ssize_t write(const void* buf, size_t count) override;

    // Gather write: append multiple buffers to the write buffer.
    ssize_t writev(const struct iovec* iov, int iovcnt) override;

    int get_fd() const override;
    bool is_open() const override;
    void close() override;

    std::shared_ptr<Address> get_local_address() const override;
    std::shared_ptr<Address> get_remote_address() const override;

    // ============================================================
    // Buffer access (zero-copy)
    // ============================================================

    // Access the read buffer directly (for zero-copy parsing).
    Buffer& read_buffer() noexcept { return read_buffer_; }
    const Buffer& read_buffer() const noexcept { return read_buffer_; }

    // Access the write buffer directly (for zero-copy construction).
    Buffer& write_buffer() noexcept { return write_buffer_; }
    const Buffer& write_buffer() const noexcept { return write_buffer_; }

    // ============================================================
    // Flush
    // ============================================================

    // Flush the write buffer to the socket.
    // Returns the number of bytes written, or -1 on error.
    // In fiber mode: yields on EAGAIN.
    ssize_t flush();

    // ============================================================
    // Socket access
    // ============================================================

    Socket::Ptr socket() const noexcept { return socket_; }

    // ============================================================
    // Configuration
    // ============================================================

    // Set the maximum read buffer size (default unlimited).
    // When exceeded, read() blocks until the buffer is drained.
    void set_read_buffer_limit(size_t limit) noexcept {
        read_buffer_limit_ = limit;
    }

    // Set the maximum write buffer size before auto-flush triggers.
    void set_write_auto_flush(size_t limit) noexcept {
        write_auto_flush_ = limit;
    }

private:
    // Perform a direct socket read (with fiber yield on EAGAIN)
    ssize_t do_read(void* buf, size_t count);

    // Perform a direct socket write (with fiber yield on EAGAIN)
    ssize_t do_write(const void* buf, size_t count);

    Socket::Ptr socket_;
    Buffer read_buffer_;
    Buffer write_buffer_;

    size_t read_buffer_limit_ = 0;    // 0 = unlimited
    size_t write_auto_flush_ = 0;     // 0 = no auto-flush
};

} // namespace zero
