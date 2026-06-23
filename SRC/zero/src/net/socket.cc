#include "zero/base/macro.h"
// zero Socket implementation
//
// Non-blocking TCP/UDP socket wrapper with RAII lifecycle management.
// All sockets are created with SOCK_NONBLOCK | SOCK_CLOEXEC by default.
//
// Key features:
//   - TCP/UDP factory methods
//   - bind/listen/accept/connect
//   - Socket option management (reuseaddr, nodelay, keepalive, etc.)
//   - Address resolution (getsockname/getpeername)
//   - Error detection (SO_ERROR)
#include "zero/net/socket.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <cstring>
#include <cerrno>

namespace zero {

// ============================================================
// Factory methods
// ============================================================
Socket::Ptr Socket::create_tcp() {
    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) return nullptr;
    return std::make_shared<Socket>(fd, Type::TCP);
}

Socket::Ptr Socket::create_udp() {
    int fd = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) return nullptr;
    return std::make_shared<Socket>(fd, Type::UDP);
}

Socket::Ptr Socket::from_fd(int fd, Type type) {
    if (fd < 0) return nullptr;

    // Detect socket type
    int sock_type = 0;
    socklen_t optlen = sizeof(sock_type);
    // type is provided via parameter
    if (::getsockopt(fd, SOL_SOCKET, SO_TYPE, &sock_type, &optlen) == 0) {
        if (sock_type == SOCK_DGRAM) {
            type = Type::UDP;
        }
    }

    // Ensure non-blocking
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags >= 0 && !(flags & O_NONBLOCK)) {
        ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    return std::make_shared<Socket>(fd, type);
}

// ============================================================
// Construction / Destruction
// ============================================================
Socket::Socket(int fd, Type type)
    : fd_(fd), type_(type) {}

Socket::~Socket() {
    close();
}

// ============================================================
// Close
// ============================================================
void Socket::close() {
    if (fd_ >= 0) {
        ::shutdown(fd_, SHUT_RDWR);
        int ret = ::close(fd_);
        ZERO_UNUSED(ret);
        fd_ = -1;
        connected_ = false;
    }
}

// ============================================================
// Bind / Listen / Accept / Connect
// ============================================================
bool Socket::bind(const Address& addr) {
    if (fd_ < 0) return false;

    int ret = ::bind(fd_, addr.addr(), addr.addr_len());
    if (ret == 0) {
        connected_ = true;
    }
    return ret == 0;
}

bool Socket::listen(int backlog) {
    if (fd_ < 0) return false;
    return ::listen(fd_, backlog) == 0;
}

Socket::Ptr Socket::accept() {
    if (fd_ < 0) return nullptr;

    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
    std::memset(&addr, 0, sizeof(addr));

    int client_fd = ::accept4(fd_,
                               reinterpret_cast<struct sockaddr*>(&addr),
                               &addrlen,
                               SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (client_fd < 0) {
        // EAGAIN means no pending connections — not an error
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            return nullptr;
        }
        return nullptr;
    }

    auto client = std::make_shared<Socket>(client_fd, type_);
    client->connected_ = true;
    return client;
}

bool Socket::connect(const Address& addr) {
    if (fd_ < 0) return false;

    int ret = ::connect(fd_, addr.addr(), addr.addr_len());
    if (ret == 0) {
        connected_ = true;
        return true;
    }

    if (errno == EINPROGRESS) {
        // Non-blocking connect in progress — caller should wait for
        // writability on the fd to confirm connection establishment.
        return true;
    }

    return false;
}

// ============================================================
// Send / Recv
// ============================================================
ssize_t Socket::send(const void* buf, size_t len, int flags) {
    if (fd_ < 0) return -1;
    return ::send(fd_, buf, len, flags | MSG_NOSIGNAL);
}

ssize_t Socket::recv(void* buf, size_t len, int flags) {
    if (fd_ < 0) return -1;
    return ::recv(fd_, buf, len, flags);
}

// ============================================================
// Socket options
// ============================================================
bool Socket::set_reuse_addr(bool on) {
    if (fd_ < 0) return false;
    int val = on ? 1 : 0;
    return ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &val,
                        static_cast<socklen_t>(sizeof(val))) == 0;
}

bool Socket::set_reuse_port(bool on) {
    if (fd_ < 0) return false;
    int val = on ? 1 : 0;
    return ::setsockopt(fd_, SOL_SOCKET, SO_REUSEPORT, &val,
                        static_cast<socklen_t>(sizeof(val))) == 0;
}

bool Socket::set_tcp_nodelay(bool on) {
    if (fd_ < 0 || type_ != Type::TCP) return false;
    int val = on ? 1 : 0;
    return ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &val,
                        static_cast<socklen_t>(sizeof(val))) == 0;
}

bool Socket::set_keepalive(bool on) {
    if (fd_ < 0) return false;
    int val = on ? 1 : 0;
    return ::setsockopt(fd_, SOL_SOCKET, SO_KEEPALIVE, &val,
                        static_cast<socklen_t>(sizeof(val))) == 0;
}

bool Socket::set_nonblocking(bool on) {
    if (fd_ < 0) return false;

    int flags = ::fcntl(fd_, F_GETFL, 0);
    if (flags < 0) return false;

    if (on) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    return ::fcntl(fd_, F_SETFL, flags) == 0;
}

bool Socket::set_send_timeout(int ms) {
    if (fd_ < 0) return false;

    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;

    return ::setsockopt(fd_, SOL_SOCKET, SO_SNDTIMEO, &tv,
                        static_cast<socklen_t>(sizeof(tv))) == 0;
}

bool Socket::set_recv_timeout(int ms) {
    if (fd_ < 0) return false;

    struct timeval tv;
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;

    return ::setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv,
                        static_cast<socklen_t>(sizeof(tv))) == 0;
}

// ============================================================
// Address queries
// ============================================================
std::shared_ptr<Address> Socket::local_address() const {
    if (fd_ < 0) return nullptr;

    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
    std::memset(&addr, 0, sizeof(addr));

    if (::getsockname(fd_, reinterpret_cast<struct sockaddr*>(&addr),
                      &addrlen) != 0) {
        return nullptr;
    }

    return Address::create(reinterpret_cast<struct sockaddr*>(&addr), addrlen);
}

std::shared_ptr<Address> Socket::peer_address() const {
    if (fd_ < 0) return nullptr;

    struct sockaddr_storage addr;
    socklen_t addrlen = sizeof(addr);
    std::memset(&addr, 0, sizeof(addr));

    if (::getpeername(fd_, reinterpret_cast<struct sockaddr*>(&addr),
                      &addrlen) != 0) {
        return nullptr;
    }

    return Address::create(reinterpret_cast<struct sockaddr*>(&addr), addrlen);
}

// ============================================================
// Error detection
// ============================================================
int Socket::get_error() const noexcept {
    if (fd_ < 0) return EBADF;

    int error = 0;
    socklen_t len = sizeof(error);
    if (::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &error, &len) != 0) {
        return errno;
    }
    return error;
}

} // namespace zero
