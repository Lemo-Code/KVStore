// zero Stream — abstract async I/O stream interface
//
// Stream defines the interface for bi-directional byte streams.
// Implementations wrap file descriptors (sockets, pipes, files) and
// provide fiber-aware async I/O — when a read/write would block,
// the calling fiber yields rather than blocking the OS thread.
//
// The interface supports:
//   - Basic read/write with scatter/gather (readv/writev)
//   - Exact-size reads/writes (read_exact, write_exact)
//   - Query local/remote addresses
//   - Close and open-state queries
//
// Concrete implementations:
//   - SocketStream: wraps a Socket (TCP connection)
//   - PipeStream: wraps a pipe fd pair
//   - FileStream: wraps a regular file fd
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <sys/uio.h>
#include <string>

namespace zero {

class Address;

class Stream {
public:
    virtual ~Stream() = default;

    // ============================================================
    // Read operations
    // ============================================================

    // Read up to `count` bytes into `buf`.
    // Returns the number of bytes read, 0 on EOF, -1 on error.
    // In fiber mode: yields on EAGAIN until data is available.
    virtual ssize_t read(void* buf, size_t count) = 0;

    // Scatter read: fill multiple buffers from the stream.
    virtual ssize_t readv(const struct iovec* iov, int iovcnt) = 0;

    // Read exactly `count` bytes. Blocks until all bytes are read
    // or an error/EOF occurs. Returns count on success, -1 on error,
    // fewer than count on early EOF.
    ssize_t read_exact(void* buf, size_t count);

    // ============================================================
    // Write operations
    // ============================================================

    // Write up to `count` bytes from `buf`.
    // Returns the number of bytes written, -1 on error.
    // In fiber mode: yields on EAGAIN until data can be sent.
    virtual ssize_t write(const void* buf, size_t count) = 0;

    // Gather write: send data from multiple buffers in one call.
    virtual ssize_t writev(const struct iovec* iov, int iovcnt) = 0;

    // Write exactly `count` bytes. Blocks until all bytes are written
    // or an error occurs. Returns count on success, -1 on error.
    ssize_t write_exact(const void* buf, size_t count);

    // ============================================================
    // Observers
    // ============================================================

    // Get the underlying file descriptor (for polling, etc.)
    virtual int get_fd() const = 0;

    // Whether the stream is open for I/O
    virtual bool is_open() const = 0;

    // Close the stream. After close(), read/write return error.
    virtual void close() = 0;

    // Get the local (bound) address of the stream
    virtual std::shared_ptr<Address> get_local_address() const {
        return nullptr;  // Optional: streams may not have addresses
    }

    // Get the remote (peer) address of the stream
    virtual std::shared_ptr<Address> get_remote_address() const {
        return nullptr;
    }

    // ============================================================
    // Convenience
    // ============================================================

    // Write a string to the stream
    ssize_t write_string(const std::string& str);

    // Read a line (up to \n) from the stream
    std::string read_line(size_t max_size = 65536);

    // Read all remaining data until EOF
    std::string read_all(size_t max_size = 1048576);
};

using StreamPtr = std::shared_ptr<Stream>;

} // namespace zero
