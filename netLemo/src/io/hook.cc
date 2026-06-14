#include "lemo/io/hook.h"

#include "lemo/utils/time_util.h"
#include "lemo/fiber/fiber.h"
#include "lemo/fiber/timer.h"
#include "lemo/io/fd_context.h"
#include "lemo/io/iomanager.h"

#include <cstdarg>
#include <cstdint>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#ifndef SYS_fcntl
#define SYS_fcntl __NR_fcntl
#endif

#include <memory>

namespace lemo {
namespace io {

namespace {

thread_local bool t_hook_enable = false;
thread_local IOManager* t_tls_iom = nullptr;
uint64_t s_connect_timeout = UINT64_MAX;

struct HookInitializer {
  HookInitializer() { hook_init(); }
};

HookInitializer s_hook_initializer;

}  // namespace

bool is_hook_enable() { return t_hook_enable; }

void set_hook_enable(bool flag) { t_hook_enable = flag; }

void set_hook_iomanager(IOManager* iom) { t_tls_iom = iom; }

IOManager* get_hook_iom() {
  return t_tls_iom != nullptr ? t_tls_iom : static_cast<IOManager*>(fiber::Scheduler::GetThis());
}

void set_connect_timeout(uint64_t timeout_ms) { s_connect_timeout = timeout_ms; }

uint64_t get_connect_timeout() { return s_connect_timeout; }

}  // namespace io
}  // namespace lemo

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
  if (!lemo::io::is_hook_enable()) {
    return fun(fd, std::forward<Args>(args)...);
  }

  lemo::io::IOManager* iom = lemo::io::get_hook_iom();
  if (iom == nullptr) {
    return fun(fd, std::forward<Args>(args)...);
  }

  lemo::io::FdContext::ptr ctx = lemo::io::FdManager::Instance().get(fd);
  if (!ctx || ctx->isClose() || !ctx->isSocket() || ctx->getUserNonBlock()) {
    return fun(fd, std::forward<Args>(args)...);
  }

  const uint64_t to = ctx->getTimeout(timeout_so);
  const bool has_timeout = to != static_cast<uint64_t>(-1);
  const auto io_event = static_cast<lemo::io::IOManager::Event>(event);

  if (!has_timeout) {
  retry_fast:
    if (ctx->isClose()) {
      errno = EBADF;
      return -1;
    }
    ssize_t n = fun(fd, std::forward<Args>(args)...);
    while (n == -1 && errno == EINTR) {
      n = fun(fd, std::forward<Args>(args)...);
    }
    if (n != -1 || errno != EAGAIN) {
      return n;
    }
    if (ctx->isClose()) {
      errno = EBADF;
      return -1;
    }
    errno = 0;
    if (iom->addEvent(fd, io_event) != 0) {
      return -1;
    }
    lemo::fiber::Fiber::YieldToHold();
    if (ctx->isClose() || ::fcntl(fd, F_GETFD) < 0) {
      errno = EBADF;
      return -1;
    }
    goto retry_fast;
  }

  TimerInfo tinfo_stack;
  TimerInfo* tinfo = &tinfo_stack;
  std::shared_ptr<TimerInfo> tinfo_heap;
  tinfo_heap.reset(new TimerInfo);
  tinfo = tinfo_heap.get();

retry:
  if (ctx->isClose()) {
    errno = EBADF;
    return -1;
  }

  ssize_t n = fun(fd, std::forward<Args>(args)...);
  while (n == -1 && errno == EINTR) {
    n = fun(fd, std::forward<Args>(args)...);
  }
  if (n != -1 || errno != EAGAIN) {
    return n;
  }

  if (ctx->isClose()) {
    errno = EBADF;
    return -1;
  }

  errno = 0;

  lemo::fiber::Timer::ptr timer;
  {
    std::weak_ptr<TimerInfo> winfo(tinfo_heap);
    timer = iom->addConditionTimer(
        to,
        [iom, fd, event, winfo]() {
          std::shared_ptr<TimerInfo> t = winfo.lock();
          if (!t || t->cancelled != 0) {
            return;
          }
          t->cancelled = ETIMEDOUT;
          iom->cancelEvent(fd, static_cast<lemo::io::IOManager::Event>(event));
        },
        std::weak_ptr<void>(), false);
  }

  if (iom->addEvent(fd, io_event) != 0) {
    if (timer) {
      timer->cancel();
    }
    return -1;
  }

  lemo::fiber::Fiber::YieldToHold();
  if (timer) {
    timer->cancel();
  }
  if (ctx->isClose() || ::fcntl(fd, F_GETFD) < 0) {
    errno = EBADF;
    return -1;
  }
  if (tinfo->cancelled != 0) {
    errno = tinfo->cancelled;
    return -1;
  }
  goto retry;
}

}  // namespace

namespace lemo {
namespace io {

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

int do_connect(int fd, const ::sockaddr* addr, socklen_t addrlen,
               uint64_t timeout_ms) {
  if (!is_hook_enable()) {
    return connect_f(fd, addr, addrlen);
  }

  FdContext::ptr ctx = FdManager::Instance().get(fd);
  if (!ctx || ctx->isClose()) {
    errno = EBADF;
    return -1;
  }
  if (!ctx->isSocket() || ctx->getUserNonBlock()) {
    return connect_f(fd, addr, addrlen);
  }

  int flags = static_cast<int>(syscall(SYS_fcntl, fd, F_GETFL, 0));
  if (flags >= 0 && !(flags & O_NONBLOCK)) {
    syscall(SYS_fcntl, fd, F_SETFL, flags | O_NONBLOCK);
    ctx->setSysNonBlock(true);
  }

  const uint64_t start_ms = lemo::utils::NowMs();
  int n = connect_f(fd, addr, addrlen);
  if (n == 0) {
    return 0;
  }
  if (n != -1 || (errno != EINPROGRESS && errno != EALREADY)) {
    return n;
  }

  lemo::io::IOManager* iom = lemo::io::get_hook_iom();
  if (iom == nullptr) {
    return connect_f(fd, addr, addrlen);
  }

  lemo::fiber::Timer::ptr timer;
  TimerInfo tinfo_stack;
  TimerInfo* tinfo = &tinfo_stack;
  std::shared_ptr<TimerInfo> tinfo_heap;
  if (timeout_ms != UINT64_MAX) {
    tinfo_heap.reset(new TimerInfo);
    tinfo = tinfo_heap.get();
    std::weak_ptr<TimerInfo> winfo(tinfo_heap);
    timer = iom->addConditionTimer(
        timeout_ms,
        [iom, fd, winfo]() {
          std::shared_ptr<TimerInfo> t = winfo.lock();
          if (!t || t->cancelled != 0) {
            return;
          }
          t->cancelled = ETIMEDOUT;
          iom->cancelEvent(fd, lemo::io::Reactor::WRITE);
        },
        std::weak_ptr<void>(), false);
  }

  if (iom->addEvent(fd, lemo::io::Reactor::WRITE) != 0) {
    if (timer) {
      timer->cancel();
    }
    return -1;
  }

  lemo::fiber::Fiber::YieldToHold();
  if (timer) {
    timer->cancel();
  }
  if (tinfo->cancelled != 0) {
    errno = tinfo->cancelled;
    return -1;
  }
  if (timeout_ms != UINT64_MAX &&
      lemo::utils::NowMs() - start_ms >= timeout_ms) {
    iom->cancelEvent(fd, lemo::io::Reactor::WRITE);
    errno = ETIMEDOUT;
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

}  // namespace io
}  // namespace lemo

extern "C" {

int connect_with_timeout(int fd, const struct sockaddr* addr, socklen_t addrlen,
                         uint64_t timeout_ms);

unsigned int sleep(unsigned int seconds) {
  if (!lemo::io::is_hook_enable()) {
    return sleep_f(seconds);
  }
  lemo::fiber::Fiber::SleepMs(static_cast<uint64_t>(seconds) * 1000);
  return 0;
}

int usleep(useconds_t usec) {
  if (!lemo::io::is_hook_enable()) {
    return usleep_f(usec);
  }
  lemo::fiber::Fiber::SleepMs(static_cast<uint64_t>(usec) / 1000);
  return 0;
}

int nanosleep(const struct timespec* req, struct timespec* rem) {
  if (!lemo::io::is_hook_enable()) {
    return nanosleep_f(req, rem);
  }
  if (req == nullptr) {
    errno = EINVAL;
    return -1;
  }
  const uint64_t ms =
      static_cast<uint64_t>(req->tv_sec) * 1000 +
      static_cast<uint64_t>(req->tv_nsec) / 1000000;
  lemo::fiber::Fiber::SleepMs(ms);
  if (rem != nullptr) {
    rem->tv_sec = 0;
    rem->tv_nsec = 0;
  }
  return 0;
}

int connect_with_timeout(int fd, const struct sockaddr* addr, socklen_t addrlen,
                         uint64_t timeout_ms) {
  return lemo::io::do_connect(fd, addr, addrlen, timeout_ms);
}

int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
  return connect_with_timeout(sockfd, addr, addrlen,
                              lemo::io::get_connect_timeout());
}

int socket(int domain, int type, int protocol) {
  if (!lemo::io::is_hook_enable()) {
    return socket_f(domain, type, protocol);
  }
  const int fd = socket_f(domain, type, protocol);
  if (fd >= 0) {
    lemo::io::FdManager::Instance().get(fd, true);
  }
  return fd;
}

int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
  const int fd = static_cast<int>(
      do_io(sockfd, accept_f, lemo::io::Reactor::READ, SO_RCVTIMEO, addr, addrlen));
  if (fd >= 0) {
    lemo::io::FdManager::Instance().get(fd, true);
  }
  return fd;
}

ssize_t read(int fd, void* buf, size_t count) {
  return do_io(fd, read_f, lemo::io::Reactor::READ, SO_RCVTIMEO, buf, count);
}

ssize_t readv(int fd, const struct iovec* iov, int iovcnt) {
  return do_io(fd, readv_f, lemo::io::Reactor::READ, SO_RCVTIMEO, iov, iovcnt);
}

ssize_t recv(int sockfd, void* buf, size_t len, int flags) {
  return do_io(sockfd, recv_f, lemo::io::Reactor::READ, SO_RCVTIMEO, buf, len,
               flags);
}

ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags,
                 struct sockaddr* src_addr, socklen_t* addrlen) {
  return do_io(sockfd, recvfrom_f, lemo::io::Reactor::READ, SO_RCVTIMEO, buf, len,
                flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags) {
  return do_io(sockfd, recvmsg_f, lemo::io::Reactor::READ, SO_RCVTIMEO, msg,
                flags);
}

ssize_t write(int fd, const void* buf, size_t count) {
  return do_io(fd, write_f, lemo::io::Reactor::WRITE, SO_SNDTIMEO, buf, count);
}

ssize_t writev(int fd, const struct iovec* iov, int iovcnt) {
  return do_io(fd, writev_f, lemo::io::Reactor::WRITE, SO_SNDTIMEO, iov, iovcnt);
}

ssize_t send(int sockfd, const void* buf, size_t len, int flags) {
  return do_io(sockfd, send_f, lemo::io::Reactor::WRITE, SO_SNDTIMEO, buf, len,
               flags);
}

ssize_t sendto(int sockfd, const void* buf, size_t len, int flags,
               const struct sockaddr* dest_addr, socklen_t addrlen) {
  return do_io(sockfd, sendto_f, lemo::io::Reactor::WRITE, SO_SNDTIMEO, buf, len,
               flags, dest_addr, addrlen);
}

ssize_t sendmsg(int sockfd, const struct msghdr* msg, int flags) {
  return do_io(sockfd, sendmsg_f, lemo::io::Reactor::WRITE, SO_SNDTIMEO, msg,
               flags);
}

int close(int fd) {
  if (!lemo::io::is_hook_enable()) {
    return close_f(fd);
  }
  lemo::io::FdContext::ptr ctx = lemo::io::FdManager::Instance().get(fd);
  lemo::io::IOManager* iom = nullptr;
  if (ctx) {
    ctx->setClose();
    iom = lemo::io::IOManager::GetThis();
    if (iom == nullptr) {
      iom = lemo::io::get_hook_iom();
    }
    if (iom != nullptr) {
      iom->cancelAll(fd);
    }
  }
  const int ret = close_f(fd);
  lemo::io::FdManager::Instance().del(fd);
  return ret;
}

int fcntl(int fd, int cmd, ...) {
  va_list va;
  va_start(va, cmd);
  switch (cmd) {
    case F_SETFL: {
      const int arg = va_arg(va, int);
      va_end(va);
      lemo::io::FdContext::ptr ctx = lemo::io::FdManager::Instance().get(fd);
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
      lemo::io::FdContext::ptr ctx = lemo::io::FdManager::Instance().get(fd);
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
    lemo::io::FdContext::ptr ctx = lemo::io::FdManager::Instance().get(fd);
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
  if (lemo::io::is_hook_enable() && level == SOL_SOCKET &&
      (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO)) {
    lemo::io::FdContext::ptr ctx = lemo::io::FdManager::Instance().get(sockfd);
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
