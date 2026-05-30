#include "socket/socket.h"

#include "io/connect.h"
#include "io/hook.h"
#include "io/fdmanager.h"
#include "io/iomanager.h"

#include <cerrno>
#include <cstring>
#include <unistd.h>

namespace net {

Socket::ptr Socket::CreateTCP(Address::ptr address) {
  return Socket::ptr(new Socket(address->getFamily(), TCP, 0));
}

Socket::ptr Socket::CreateUDP(Address::ptr address) {
  Socket::ptr sock(new Socket(address->getFamily(), UDP, 0));
  sock->newSock();
  sock->is_connected_ = true;
  return sock;
}

Socket::ptr Socket::CreateTCPSocket() {
  return Socket::ptr(new Socket(IPv4, TCP, 0));
}

Socket::ptr Socket::CreateUDPSocket() {
  Socket::ptr sock(new Socket(IPv4, UDP, 0));
  sock->newSock();
  sock->is_connected_ = true;
  return sock;
}

Socket::ptr Socket::CreateTCPSocket6() {
  return Socket::ptr(new Socket(IPv6, TCP, 0));
}

Socket::ptr Socket::CreateUDPSocket6() {
  Socket::ptr sock(new Socket(IPv6, UDP, 0));
  sock->newSock();
  sock->is_connected_ = true;
  return sock;
}

Socket::ptr Socket::CreateUnixTCPSocket() {
  return Socket::ptr(new Socket(UNIX, TCP, 0));
}

Socket::ptr Socket::CreateUnixUDPSocket() {
  return Socket::ptr(new Socket(UNIX, UDP, 0));
}

Socket::Socket(int family, int type, int protocol)
    : sock_(-1),
      family_(family),
      type_(type),
      protocol_(protocol),
      is_connected_(false) {}

Socket::~Socket() { close(); }

int64_t Socket::getSendTimeout() {
  FdCtx::ptr ctx = FdMgr::GetInstance()->get(sock_);
  if (ctx) {
    return static_cast<int64_t>(ctx->getTimeout(SO_SNDTIMEO));
  }
  return -1;
}

void Socket::setSendTimeout(int64_t v) {
  struct timeval tv {
    static_cast<int>(v / 1000), static_cast<int>(v % 1000 * 1000)
  };
  setOption(SOL_SOCKET, SO_SNDTIMEO, tv);
}

int64_t Socket::getRecvTimeout() {
  FdCtx::ptr ctx = FdMgr::GetInstance()->get(sock_);
  if (ctx) {
    return static_cast<int64_t>(ctx->getTimeout(SO_RCVTIMEO));
  }
  return -1;
}

void Socket::setRecvTimeout(int64_t v) {
  struct timeval tv {
    static_cast<int>(v / 1000), static_cast<int>(v % 1000 * 1000)
  };
  setOption(SOL_SOCKET, SO_RCVTIMEO, tv);
}

bool Socket::getOption(int level, int option, void* result, size_t* len) {
  return ::getsockopt(sock_, level, option, result,
                      reinterpret_cast<socklen_t*>(len)) == 0;
}

bool Socket::setOption(int level, int option, const void* result, size_t len) {
  return ::setsockopt(sock_, level, option, result,
                      static_cast<socklen_t>(len)) == 0;
}

Socket::ptr Socket::accept() {
  Socket::ptr sock(new Socket(family_, type_, protocol_));
  int newsock = ::accept(sock_, nullptr, nullptr);
  if (newsock == -1) {
    return nullptr;
  }
  if (sock->init(newsock)) {
    return sock;
  }
  return nullptr;
}

bool Socket::init(int sock) {
  FdCtx::ptr ctx = FdMgr::GetInstance()->get(sock, true);
  if (ctx && ctx->isSocket() && !ctx->isClose()) {
    sock_ = sock;
    is_connected_ = true;
    initSock();
    getLocalAddress();
    getRemoteAddress();
    return true;
  }
  return false;
}

bool Socket::bind(const Address::ptr addr) {
  if (!isValid()) {
    newSock();
    if (!isValid()) {
      return false;
    }
  }

  if (addr->getFamily() != family_) {
    return false;
  }

  if (::bind(sock_, addr->getAddr(), addr->getAddrLen()) != 0) {
    return false;
  }
  getLocalAddress();
  return true;
}

bool Socket::connect(const Address::ptr addr, uint64_t timeout_ms) {
  remote_address_ = addr;
  if (!isValid()) {
    newSock();
    if (!isValid()) {
      return false;
    }
  }

  if (addr->getFamily() != family_) {
    return false;
  }

  if (timeout_ms == UINT64_MAX) {
    if (::connect(sock_, addr->getAddr(), addr->getAddrLen()) != 0) {
      close();
      return false;
    }
  } else if (connectWithTimeout(sock_, addr->getAddr(), addr->getAddrLen(),
                                timeout_ms) != 0) {
    close();
    return false;
  }

  is_connected_ = true;
  getRemoteAddress();
  getLocalAddress();
  return true;
}

bool Socket::listen(int backlog) {
  if (!isValid()) {
    return false;
  }
  return ::listen(sock_, backlog) == 0;
}

bool Socket::close() {
  if (!is_connected_ && sock_ == -1) {
    return true;
  }
  is_connected_ = false;
  if (sock_ != -1) {
    ::close(sock_);
    sock_ = -1;
  }
  return false;
}

bool Socket::isClose() const { return !is_connected_ && sock_ == -1; }

int Socket::send(const void* buffers, size_t length, int flags) {
  if (isConnected()) {
    return static_cast<int>(::send(sock_, buffers, length, flags));
  }
  return -1;
}

int Socket::send(const iovec* buffers, size_t length, int flags) {
  if (!isConnected()) {
    return -1;
  }
  msghdr msg;
  std::memset(&msg, 0, sizeof(msg));
  msg.msg_iov = const_cast<iovec*>(buffers);
  msg.msg_iovlen = length;
  return static_cast<int>(::sendmsg(sock_, &msg, flags));
}

int Socket::sendTo(const void* buffers, size_t length, const Address::ptr to,
                   int flags) {
  if (isConnected()) {
    return static_cast<int>(
        ::sendto(sock_, buffers, length, flags, to->getAddr(), to->getAddrLen()));
  }
  return -1;
}

int Socket::sendTo(const iovec* buffers, size_t length, const Address::ptr to,
                   int flags) {
  if (!isConnected()) {
    return -1;
  }
  msghdr msg;
  std::memset(&msg, 0, sizeof(msg));
  msg.msg_iov = const_cast<iovec*>(buffers);
  msg.msg_iovlen = length;
  msg.msg_name = to->getAddr();
  msg.msg_namelen = to->getAddrLen();
  return static_cast<int>(::sendmsg(sock_, &msg, flags));
}

int Socket::recv(void* buffer, size_t length, int flags) {
  if (isConnected()) {
    return static_cast<int>(::recv(sock_, buffer, length, flags));
  }
  return -1;
}

int Socket::recv(iovec* buffer, size_t length, int flags) {
  if (!isConnected()) {
    return -1;
  }
  msghdr msg;
  std::memset(&msg, 0, sizeof(msg));
  msg.msg_iov = buffer;
  msg.msg_iovlen = length;
  return static_cast<int>(::recvmsg(sock_, &msg, flags));
}

int Socket::recvFrom(void* buffer, size_t length, Address::ptr from,
                     int flags) {
  if (!isConnected()) {
    return -1;
  }
  socklen_t len = from->getAddrLen();
  return static_cast<int>(
      ::recvfrom(sock_, buffer, length, flags, from->getAddr(), &len));
}

int Socket::recvFrom(iovec* buffer, size_t length, Address::ptr from,
                     int flags) {
  if (!isConnected()) {
    return -1;
  }
  msghdr msg;
  std::memset(&msg, 0, sizeof(msg));
  msg.msg_iov = buffer;
  msg.msg_iovlen = length;
  msg.msg_name = from->getAddr();
  msg.msg_namelen = from->getAddrLen();
  return static_cast<int>(::recvmsg(sock_, &msg, flags));
}

Address::ptr Socket::getRemoteAddress() {
  if (remote_address_) {
    return remote_address_;
  }

  Address::ptr result;
  switch (family_) {
    case AF_INET:
      result.reset(new IPv4Address());
      break;
    case AF_INET6:
      result.reset(new IPv6Address());
      break;
    case AF_UNIX:
      result.reset(new UnixAddress());
      break;
    default:
      result.reset(new UnknownAddress(family_));
      break;
  }

  socklen_t addrlen = result->getAddrLen();
  if (::getpeername(sock_, result->getAddr(), &addrlen) != 0) {
    return Address::ptr(new UnknownAddress(family_));
  }
  if (family_ == AF_UNIX) {
    UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
    addr->setAddrLen(addrlen);
  }
  remote_address_ = result;
  return remote_address_;
}

Address::ptr Socket::getLocalAddress() {
  if (local_address_) {
    return local_address_;
  }

  Address::ptr result;
  switch (family_) {
    case AF_INET:
      result.reset(new IPv4Address());
      break;
    case AF_INET6:
      result.reset(new IPv6Address());
      break;
    case AF_UNIX:
      result.reset(new UnixAddress());
      break;
    default:
      result.reset(new UnknownAddress(family_));
      break;
  }

  socklen_t addrlen = result->getAddrLen();
  if (::getsockname(sock_, result->getAddr(), &addrlen) != 0) {
    return Address::ptr(new UnknownAddress(family_));
  }
  if (family_ == AF_UNIX) {
    UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
    addr->setAddrLen(addrlen);
  }
  local_address_ = result;
  return local_address_;
}

bool Socket::isValid() const { return sock_ != -1; }

int Socket::getError() {
  int error = 0;
  size_t len = sizeof(error);
  if (!getOption(SOL_SOCKET, SO_ERROR, &error, &len)) {
    return -1;
  }
  return error;
}

std::ostream& Socket::dump(std::ostream& os) const {
  os << "[Socket sock=" << sock_ << " is_connected=" << is_connected_
     << " family=" << family_ << " type=" << type_ << " protocol=" << protocol_
     << " ";
  if (local_address_) {
    os << local_address_->toString() << " ";
  }
  if (remote_address_) {
    os << remote_address_->toString();
  }
  os << "]";
  return os;
}

bool Socket::cancelRead() {
  IOManager* iom = IOManager::GetThis();
  return iom && iom->cancelEvent(sock_, IOManager::READ);
}

bool Socket::cancelWrite() {
  IOManager* iom = IOManager::GetThis();
  return iom && iom->cancelEvent(sock_, IOManager::WRITE);
}

bool Socket::cancelAccept() {
  IOManager* iom = IOManager::GetThis();
  return iom && iom->cancelEvent(sock_, IOManager::READ);
}

bool Socket::cancelAll() {
  IOManager* iom = IOManager::GetThis();
  return iom && iom->cancelAll(sock_);
}

void Socket::initSock() {
  int val = 1;
  setOption(SOL_SOCKET, SO_REUSEADDR, val);
  if (type_ == SOCK_STREAM) {
    setOption(IPPROTO_TCP, TCP_NODELAY, val);
  }
}

void Socket::newSock() {
  sock_ = ::socket(family_, type_, protocol_);
  if (sock_ != -1) {
    FdMgr::GetInstance()->get(sock_, true);
    initSock();
  }
}

std::ostream& operator<<(std::ostream& os, const Socket& sock) {
  return sock.dump(os);
}

}  // namespace net
