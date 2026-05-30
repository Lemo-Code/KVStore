#include "io/hook.h"

#include "fiber/fiber.h"
#include "fiber/timer.h"
#include "io/fdmanager.h"
#include "io/iomanager.h"

#include <cstdarg>
#include <cstdint>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

#include <memory>

namespace net {

namespace {

thread_local bool t_hook_enable = false;
uint64_t s_connect_timeout = UINT64_MAX;

struct HookInitializer {
  HookInitializer() { hook_init(); }
};

HookInitializer s_hook_initializer;

}  // namespace

bool is_hook_enable() { return t_hook_enable; }

void set_hook_enable(bool flag) { t_hook_enable = flag; }

void set_connect_timeout(uint64_t timeout_ms) { s_connect_timeout = timeout_ms; }

uint64_t get_connect_timeout() { return s_connect_timeout; }

}  // namespace net

#define HOOK_FUN(XX)       \
  XX(sleep)                \
  XX(usleep)               \
  XX(nanosleep)            \
  XX(socket)               \
  XX(connect)              \
  XX(accept)               \
  XX(read)                 \
  XX(recv)                 \
  XX(recvfrom)             \
  XX(recvmsg)              \
  XX(write)                \
  XX(send)                 \
  XX(sendto)               \
  XX(sendmsg)              \
  XX(close)                \
  XX(fcntl)                \
  XX(ioctl)                \
  XX(getsockopt)           \
  XX(setsockopt)

extern "C" {

#define XX(name) typedef decltype(&name) name##_fun;
HOOK_FUN(XX)
#undef XX

#define XX(name) name##_fun name##_f = nullptr;
HOOK_FUN(XX)
#undef XX

typedef ssize_t (*readv_fun)(int, const struct iovec*, int);
typedef ssize_t (*writev_fun)(int, const struct iovec*, int);
readv_fun readv_f = nullptr;
writev_fun writev_f = nullptr;

}  // extern "C"

struct TimerInfo {
  int cancelled = 0;
};

namespace {

template <typename OriginFun, typename... Args>
ssize_t do_io(int fd, OriginFun fun, uint32_t event, int timeout_so,
              Args&&... args) {
  if (!net::is_hook_enable()) {
    return fun(fd, std::forward<Args>(args)...);
  }

  net::FdCtx::ptr ctx = net::FdMgr::GetInstance()->get(fd);
  if (!ctx) {
    return fun(fd, std::forward<Args>(args)...);
  }
  if (ctx->isClose()) {
    errno = EBADF;
    return -1;
  }
  if (!ctx->isSocket() || ctx->getUserNonBlock()) {
    return fun(fd, std::forward<Args>(args)...);
  }

  uint64_t to = ctx->getTimeout(timeout_so);
  std::shared_ptr<TimerInfo> tinfo(new TimerInfo);

retry:
  ssize_t n = fun(fd, std::forward<Args>(args)...);
  while (n == -1 && errno == EINTR) {
    n = fun(fd, std::forward<Args>(args)...);
  }
  if (n != -1 || errno != EAGAIN) {
    return n;
  }

  errno = 0;
  net::IOManager* iom = net::IOManager::GetThis();
  if (iom == nullptr) {
    return fun(fd, std::forward<Args>(args)...);
  }

  net::Timer::ptr timer;
  std::weak_ptr<TimerInfo> winfo(tinfo);
  if (to != static_cast<uint64_t>(-1)) {
    timer = iom->addConditionTimer(
        to,
        [iom, fd, event, winfo]() {
          std::shared_ptr<TimerInfo> t = winfo.lock();
          if (!t || t->cancelled != 0) {
            return;
          }
          t->cancelled = ETIMEDOUT;
          iom->cancelEvent(fd, static_cast<net::IOManager::Event>(event));
        },
        std::weak_ptr<void>(), false);
  }

  if (iom->addEvent(fd, static_cast<net::IOManager::Event>(event)) != 0) {
    if (timer) {
      timer->cancel();
    }
    return -1;
  }

  net::Fiber::YieldToHold();
  if (timer) {
    timer->cancel();
  }
  if (tinfo->cancelled != 0) {
    errno = tinfo->cancelled;
    return -1;
  }
  goto retry;
}

}  // namespace

namespace net {

void hook_init() {
  static bool inited = false;
  if (inited) {
    return;
  }
#define XX(name) name##_f = (name##_fun)dlsym(RTLD_NEXT, #name);
  HOOK_FUN(XX)
#undef XX
  readv_f = (readv_fun)dlsym(RTLD_NEXT, "readv");
  writev_f = (writev_fun)dlsym(RTLD_NEXT, "writev");
  inited = true;
}

int do_connect(int fd, const struct sockaddr* addr, socklen_t addrlen,
               uint64_t timeout_ms) {
  if (!is_hook_enable()) {
    return connect_f(fd, addr, addrlen);
  }

  FdCtx::ptr ctx = FdMgr::GetInstance()->get(fd);
  if (!ctx || ctx->isClose()) {
    errno = EBADF;
    return -1;
  }
  if (!ctx->isSocket() || ctx->getUserNonBlock()) {
    return connect_f(fd, addr, addrlen);
  }

  int n = connect_f(fd, addr, addrlen);
  if (n == 0) {
    return 0;
  }
  if (n != -1 || errno != EINPROGRESS) {
    return n;
  }

  IOManager* iom = IOManager::GetThis();
  if (iom == nullptr) {
    return connect_f(fd, addr, addrlen);
  }

  Timer::ptr timer;
  std::shared_ptr<TimerInfo> tinfo(new TimerInfo);
  std::weak_ptr<TimerInfo> winfo(tinfo);

  if (timeout_ms != UINT64_MAX) {
    timer = iom->addConditionTimer(
        timeout_ms,
        [iom, fd, winfo]() {
          std::shared_ptr<TimerInfo> t = winfo.lock();
          if (!t || t->cancelled != 0) {
            return;
          }
          t->cancelled = ETIMEDOUT;
          iom->cancelEvent(fd, IOManager::WRITE);
        },
        std::weak_ptr<void>(), false);
  }

  if (iom->addEvent(fd, IOManager::WRITE) != 0) {
    if (timer) {
      timer->cancel();
    }
    return -1;
  }

  net::Fiber::YieldToHold();
  if (timer) {
    timer->cancel();
  }
  if (tinfo->cancelled != 0) {
    errno = tinfo->cancelled;
    return -1;
  }

  int error = 0;
  socklen_t len = sizeof(error);
  if (getsockopt_f(fd, SOL_SOCKET, SO_ERROR, &error, &len) != 0) {
    return -1;
  }
  if (error == 0) {
    return 0;
  }
  errno = error;
  return -1;
}

}  // namespace net

extern "C" {

int connect_with_timeout(int fd, const struct sockaddr* addr, socklen_t addrlen,
                         uint64_t timeout_ms);

unsigned int sleep(unsigned int seconds) {
  if (!net::is_hook_enable()) {
    return sleep_f(seconds);
  }
  net::Fiber::SleepMs(static_cast<uint64_t>(seconds) * 1000);
  return 0;
}

int usleep(useconds_t usec) {
  if (!net::is_hook_enable()) {
    return usleep_f(usec);
  }
  net::Fiber::SleepMs(static_cast<uint64_t>(usec) / 1000);
  return 0;
}

int nanosleep(const struct timespec* req, struct timespec* rem) {
  if (!net::is_hook_enable()) {
    return nanosleep_f(req, rem);
  }
  if (req == nullptr) {
    errno = EINVAL;
    return -1;
  }
  const uint64_t ms =
      static_cast<uint64_t>(req->tv_sec) * 1000 +
      static_cast<uint64_t>(req->tv_nsec) / 1000000;
  net::Fiber::SleepMs(ms);
  if (rem != nullptr) {
    rem->tv_sec = 0;
    rem->tv_nsec = 0;
  }
  return 0;
}

int connect_with_timeout(int fd, const struct sockaddr* addr, socklen_t addrlen,
                         uint64_t timeout_ms) {
  return net::do_connect(fd, addr, addrlen, timeout_ms);
}

int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
  return connect_with_timeout(sockfd, addr, addrlen,
                              net::get_connect_timeout());
}

int socket(int domain, int type, int protocol) {
  if (!net::is_hook_enable()) {
    return socket_f(domain, type, protocol);
  }
  const int fd = socket_f(domain, type, protocol);
  if (fd >= 0) {
    net::FdMgr::GetInstance()->get(fd, true);
  }
  return fd;
}

int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
  const int fd = static_cast<int>(
      do_io(sockfd, accept_f, net::IOManager::READ, SO_RCVTIMEO, addr, addrlen));
  if (fd >= 0) {
    net::FdMgr::GetInstance()->get(fd, true);
  }
  return fd;
}

ssize_t read(int fd, void* buf, size_t count) {
  return do_io(fd, read_f, net::IOManager::READ, SO_RCVTIMEO, buf, count);
}

ssize_t readv(int fd, const struct iovec* iov, int iovcnt) {
  return do_io(fd, readv_f, net::IOManager::READ, SO_RCVTIMEO, iov, iovcnt);
}

ssize_t recv(int sockfd, void* buf, size_t len, int flags) {
  return do_io(sockfd, recv_f, net::IOManager::READ, SO_RCVTIMEO, buf, len,
               flags);
}

ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags,
                 struct sockaddr* src_addr, socklen_t* addrlen) {
  return do_io(sockfd, recvfrom_f, net::IOManager::READ, SO_RCVTIMEO, buf, len,
                flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags) {
  return do_io(sockfd, recvmsg_f, net::IOManager::READ, SO_RCVTIMEO, msg,
                flags);
}

ssize_t write(int fd, const void* buf, size_t count) {
  return do_io(fd, write_f, net::IOManager::WRITE, SO_SNDTIMEO, buf, count);
}

ssize_t writev(int fd, const struct iovec* iov, int iovcnt) {
  return do_io(fd, writev_f, net::IOManager::WRITE, SO_SNDTIMEO, iov, iovcnt);
}

ssize_t send(int sockfd, const void* buf, size_t len, int flags) {
  return do_io(sockfd, send_f, net::IOManager::WRITE, SO_SNDTIMEO, buf, len,
               flags);
}

ssize_t sendto(int sockfd, const void* buf, size_t len, int flags,
               const struct sockaddr* dest_addr, socklen_t addrlen) {
  return do_io(sockfd, sendto_f, net::IOManager::WRITE, SO_SNDTIMEO, buf, len,
               flags, dest_addr, addrlen);
}

ssize_t sendmsg(int sockfd, const struct msghdr* msg, int flags) {
  return do_io(sockfd, sendmsg_f, net::IOManager::WRITE, SO_SNDTIMEO, msg,
               flags);
}

int close(int fd) {
  if (!net::is_hook_enable()) {
    return close_f(fd);
  }
  net::FdCtx::ptr ctx = net::FdMgr::GetInstance()->get(fd);
  if (ctx) {
    net::IOManager* iom = net::IOManager::GetThis();
    if (iom != nullptr) {
      iom->cancelAll(fd);
    }
    net::FdMgr::GetInstance()->del(fd);
  }
  return close_f(fd);
}

int fcntl(int fd, int cmd, ...) {
  va_list va;
  va_start(va, cmd);
  switch (cmd) {
    case F_SETFL: {
      const int arg = va_arg(va, int);
      va_end(va);
      net::FdCtx::ptr ctx = net::FdMgr::GetInstance()->get(fd);
      if (!ctx || ctx->isClose()) {
        return fcntl_f(fd, cmd, arg);
      }
      ctx->setUserNonBlock((arg & O_NONBLOCK) != 0);
      int real_arg = arg;
      if (ctx->getSysNonBlock()) {
        real_arg |= O_NONBLOCK;
      } else {
        real_arg &= ~O_NONBLOCK;
      }
      return fcntl_f(fd, cmd, real_arg);
    }
    case F_GETFL: {
      va_end(va);
      int arg = fcntl_f(fd, cmd);
      net::FdCtx::ptr ctx = net::FdMgr::GetInstance()->get(fd);
      if (!ctx || ctx->isClose() || !ctx->isSocket()) {
        return arg;
      }
      if (ctx->getUserNonBlock()) {
        return arg | O_NONBLOCK;
      }
      return arg & ~O_NONBLOCK;
    }
    default:
      break;
  }
  va_end(va);
  va_start(va, cmd);
  void* arg = va_arg(va, void*);
  va_end(va);
  return fcntl_f(fd, cmd, arg);
}

int ioctl(int fd, unsigned long request, ...) {
  va_list va;
  va_start(va, request);
  void* arg = va_arg(va, void*);
  va_end(va);

  if (request == FIONBIO && arg != nullptr) {
    net::FdCtx::ptr ctx = net::FdMgr::GetInstance()->get(fd);
    if (ctx && !ctx->isClose() && ctx->isSocket()) {
      ctx->setUserNonBlock(*static_cast<int*>(arg) != 0);
    }
  }
  return ioctl_f(fd, request, arg);
}

int getsockopt(int sockfd, int level, int optname, void* optval,
               socklen_t* optlen) {
  return getsockopt_f(sockfd, level, optname, optval, optlen);
}

int setsockopt(int sockfd, int level, int optname, const void* optval,
               socklen_t optlen) {
  if (net::is_hook_enable() && level == SOL_SOCKET &&
      (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO)) {
    net::FdCtx::ptr ctx = net::FdMgr::GetInstance()->get(sockfd);
    if (ctx && optval != nullptr &&
        optlen >= static_cast<socklen_t>(sizeof(timeval))) {
      const timeval* tv = static_cast<const timeval*>(optval);
      const uint64_t ms =
          static_cast<uint64_t>(tv->tv_sec) * 1000 +
          static_cast<uint64_t>(tv->tv_usec) / 1000;
      ctx->setTimeout(optname, ms);
    }
  }
  return setsockopt_f(sockfd, level, optname, optval, optlen);
}

}  // extern "C"
