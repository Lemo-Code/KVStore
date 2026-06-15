#include "zero/net/socket.h"
#include "zero/scheduler/hook.h"
#include "zero/scheduler/fd_manager.h"

#include <unistd.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>

namespace zero {

// ====================================================================
// 工厂
// ====================================================================
Socket::ptr Socket::CreateTCP(Address::ptr addr) {
    auto sock = std::make_shared<Socket>(addr->getFamily(), TCP);
    if (!sock->init()) return nullptr;
    if (addr && !sock->bind(addr)) return nullptr;
    return sock;
}

Socket::ptr Socket::CreateUDP(Address::ptr addr) {
    auto sock = std::make_shared<Socket>(addr->getFamily(), UDP);
    if (!sock->init()) return nullptr;
    if (addr && !sock->bind(addr)) return nullptr;
    return sock;
}

Socket::ptr Socket::CreateTCPSocket(int family) {
    auto sock = std::make_shared<Socket>(family, TCP);
    return sock->init() ? sock : nullptr;
}

Socket::ptr Socket::CreateUDPSocket(int family) {
    auto sock = std::make_shared<Socket>(family, UDP);
    return sock->init() ? sock : nullptr;
}

// ====================================================================
// 构造/析构
// ====================================================================
Socket::Socket(int family, int type, int protocol)
    : family_(family), type_(type), protocol_(protocol) {}

Socket::~Socket() {
    close();
}

bool Socket::init(int sock) {
    if (sock >= 0) {
        sock_ = sock;
        is_closed_ = false;
    } else {
        sock_ = ::socket(family_, type_, protocol_);
        if (sock_ < 0) return false;
        is_closed_ = false;
    }

    // 注册到 FdManager
    auto fd_ctx = FdMgr::GetInstance()->get(sock_, true);
    if (fd_ctx) {
        fd_ctx->setSysNonblock(true);  // 标记为系统级 nonblock (非用户意图)
    }

    // 直接使用 syscall 设置 nonblock (完全绕过 hook)
    int flags = syscall(SYS_fcntl, sock_, F_GETFL, 0);
    if (flags >= 0) {
        syscall(SYS_fcntl, sock_, F_SETFL, flags | O_NONBLOCK);
    }

    // TCP_NODELAY
    if (type_ == SOCK_STREAM) {
        setTcpNoDelay(true);
    }

    return true;
}

bool Socket::bind(const Address::ptr addr) {
    if (is_closed_ || !addr) return false;
    int rt = ::bind(sock_, addr->getAddr(), addr->getAddrLen());
    if (rt == 0) local_addr_ = addr;
    return rt == 0;
}

bool Socket::listen(int backlog) {
    if (is_closed_) return false;
    return ::listen(sock_, backlog) == 0;
}

Socket::ptr Socket::accept() {
    if (is_closed_) return nullptr;

    sockaddr_storage addr;
    socklen_t len = sizeof(addr);
    int fd = ::accept(sock_, (sockaddr*)&addr, &len);

    if (fd < 0) return nullptr;

    auto sock = std::make_shared<Socket>(family_, type_, protocol_);
    if (!sock->init(fd)) {
        ::close(fd);
        return nullptr;
    }
    sock->remote_addr_ = Address::Create((sockaddr*)&addr, len);
    sock->is_connected_ = true;
    return sock;
}

bool Socket::connect(const Address::ptr addr, uint64_t timeout_ms) {
    if (is_closed_ || !addr) return false;

    int rt = ::connect(sock_, addr->getAddr(), addr->getAddrLen());
    if (rt == 0) {
        remote_addr_ = addr;
        is_connected_ = true;
        return true;
    }
    // Nonblock connect: EINPROGRESS is expected
    if (errno == EINPROGRESS) {
        // Wait for writability (handled by hook's async connect)
        // For now, just return true — the hook layer handles the async wait
        remote_addr_ = addr;
        is_connected_ = true;
        return true;
    }
    return false;
}

bool Socket::close() {
    if (!is_closed_) {
        ::close(sock_);
        FdMgr::GetInstance()->del(sock_);
        is_closed_ = true;
        is_connected_ = false;
    }
    return true;
}

// ====================================================================
// IO
// ====================================================================
ssize_t Socket::send(const void* buf, size_t len, int flags) {
    if (is_closed_) return -1;
    return ::send(sock_, buf, len, flags);
}

ssize_t Socket::recv(void* buf, size_t len, int flags) {
    if (is_closed_) return -1;
    return ::recv(sock_, buf, len, flags);
}

// ====================================================================
// 选项
// ====================================================================
int64_t Socket::getSendTimeout() {
    FdCtx::ptr ctx = FdMgr::GetInstance()->get(sock_);
    return ctx ? static_cast<int64_t>(ctx->getTimeout(FdCtx::SEND_TIMEOUT)) : -1;
}

void Socket::setSendTimeout(int64_t ms) {
    FdCtx::ptr ctx = FdMgr::GetInstance()->get(sock_);
    if (ctx) ctx->setTimeout(FdCtx::SEND_TIMEOUT, ms);
}

int64_t Socket::getRecvTimeout() {
    FdCtx::ptr ctx = FdMgr::GetInstance()->get(sock_);
    return ctx ? static_cast<int64_t>(ctx->getTimeout(FdCtx::RECV_TIMEOUT)) : -1;
}

void Socket::setRecvTimeout(int64_t ms) {
    FdCtx::ptr ctx = FdMgr::GetInstance()->get(sock_);
    if (ctx) ctx->setTimeout(FdCtx::RECV_TIMEOUT, ms);
}

bool Socket::getOption(int level, int optname, void* result, size_t* len) {
    socklen_t slen = static_cast<socklen_t>(*len);
    if (getsockopt(sock_, level, optname, result, &slen) != 0) return false;
    *len = slen;
    return true;
}

bool Socket::setOption(int level, int optname, const void* val, size_t len) {
    return setsockopt(sock_, level, optname, val, static_cast<socklen_t>(len)) == 0;
}

void Socket::setTcpNoDelay(bool on) {
    int val = on ? 1 : 0;
    setOption(IPPROTO_TCP, TCP_NODELAY, val);
}

Address::ptr Socket::getRemoteAddress() {
    if (remote_addr_) return remote_addr_;
    if (is_closed_) return nullptr;

    sockaddr_storage addr;
    socklen_t len = sizeof(addr);
    if (getpeername(sock_, (sockaddr*)&addr, &len) == 0) {
        remote_addr_ = Address::Create((sockaddr*)&addr, len);
    }
    return remote_addr_;
}

Address::ptr Socket::getLocalAddress() {
    if (local_addr_) return local_addr_;
    if (is_closed_) return nullptr;

    sockaddr_storage addr;
    socklen_t len = sizeof(addr);
    if (getsockname(sock_, (sockaddr*)&addr, &len) == 0) {
        local_addr_ = Address::Create((sockaddr*)&addr, len);
    }
    return local_addr_;
}

bool Socket::isValid() const {
    if (is_closed_) return false;
    int error = 0;
    socklen_t len = sizeof(error);
    return getsockopt(sock_, SOL_SOCKET, SO_ERROR, &error, &len) == 0 && error == 0;
}

int Socket::getError() {
    int error = 0;
    socklen_t len = sizeof(error);
    getsockopt(sock_, SOL_SOCKET, SO_ERROR, &error, &len);
    return error;
}

} // namespace zero
