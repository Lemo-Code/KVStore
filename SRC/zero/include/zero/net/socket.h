// zero Socket — non-blocking TCP/UDP socket wrapper
//
// Provides a clean RAII interface around file descriptors for network
// sockets. When hooks are enabled (fiber mode), all I/O operations
// are transparently made async — the calling fiber yields rather than
// blocking the OS thread.
//
// Features:
//   - Factory creation (createTCP, createUDP, fromFd)
//   - Full set of socket options (reuseaddr, reuseport, nodelay, keepalive)
//   - Non-blocking mode management
//   - Timeout support
//   - Local/peer address retrieval
//   - Error checking via SO_ERROR
//
// Move-only; shared ownership via shared_ptr (Ptr).
#pragma once

#include <memory>
#include <string>

#include "zero/net/address.h"
#include "zero/base/noncopyable.h"

namespace zero {

class Socket : public Noncopyable,
               public std::enable_shared_from_this<Socket> {
public:
    enum class Type { TCP, UDP };

    using Ptr = std::shared_ptr<Socket>;

    // ============================================================
    // Factory methods (preferred over constructors)
    // ============================================================

    // Create a new unbound TCP socket
    static Ptr create_tcp();

    // Create a new unbound UDP socket
    static Ptr create_udp();

    // Create a new unbound socket for the given address family and type
    static Ptr create(int family, Type type);

    // Wrap an existing file descriptor
    static Ptr from_fd(int fd, Type type = Type::TCP);

    // ============================================================
    // Lifecycle
    // ============================================================

    ~Socket();

    // Close the socket. Safe to call multiple times.
    void close();

    // ============================================================
    // Binding / Listening / Accepting
    // ============================================================

    // Bind the socket to a local address
    bool bind(const Address& addr);

    // Listen for incoming connections (TCP only)
    bool listen(int backlog = 128);

    // Accept a new connection (TCP only).
    // Returns a new Socket for the accepted connection.
    Ptr accept();

    // Connect to a remote address (TCP and UDP)
    bool connect(const Address& addr);

    // ============================================================
    // Socket options
    // ============================================================

    bool set_reuse_addr(bool on);
    bool set_reuse_port(bool on);
    bool set_tcp_nodelay(bool on);
    bool set_keepalive(bool on);
    bool set_nonblocking(bool on);
    bool set_send_timeout(int ms);
    bool set_recv_timeout(int ms);
    bool set_send_buffer_size(int size);
    bool set_recv_buffer_size(int size);
    bool set_linger(bool on, int seconds = 0);
    bool set_ttl(int ttl);

    bool get_reuse_addr() const noexcept;
    bool get_tcp_nodelay() const noexcept;
    bool get_keepalive() const noexcept;
    bool is_nonblocking() const noexcept;

    // ============================================================
    // I/O operations
    // ============================================================

    ssize_t send(const void* buf, size_t len, int flags = 0);
    ssize_t recv(void* buf, size_t len, int flags = 0);
    ssize_t send_to(const void* buf, size_t len,
                     const Address& dest, int flags = 0);
    ssize_t recv_from(void* buf, size_t len,
                       std::shared_ptr<Address>& src, int flags = 0);

    // ============================================================
    // Address retrieval
    // ============================================================

    std::shared_ptr<Address> local_address() const;
    std::shared_ptr<Address> peer_address() const;

    // ============================================================
    // Observers
    // ============================================================

    int fd() const noexcept { return fd_; }
    Type type() const noexcept { return type_; }

    // Check for pending socket errors (SO_ERROR)
    int get_error() const noexcept;

    // Whether the socket is valid (fd != -1)
    bool is_valid() const noexcept { return fd_ >= 0; }

    // Whether the socket is connected (TCP)
    bool is_connected() const noexcept { return connected_; }

    // ============================================================
    // Constructor (public for make_shared, but prefer factory methods)
    // ============================================================

    Socket(int fd, Type type);

private:
    int fd_;
    Type type_;
    bool connected_ = false;
};

} // namespace zero
