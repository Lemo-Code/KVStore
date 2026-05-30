#include "io/fdmanager.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef SYS_fcntl
#define SYS_fcntl __NR_fcntl
#endif

namespace net {

FdCtx::FdCtx(int fd)
    : is_init_(false),
      is_socket_(false),
      sys_nonblock_(false),
      user_nonblock_(false),
      is_closed_(false),
      fd_(fd),
      recv_timeout_(static_cast<uint64_t>(-1)),
      send_timeout_(static_cast<uint64_t>(-1)) {
  init();
}

FdCtx::~FdCtx() {}

bool FdCtx::init() {
  if (is_init_) {
    return true;
  }
  recv_timeout_ = static_cast<uint64_t>(-1);
  send_timeout_ = static_cast<uint64_t>(-1);

  struct stat fd_stat;
  if (::fstat(fd_, &fd_stat) == -1) {
    is_init_ = false;
    is_socket_ = false;
  } else {
    is_init_ = true;
    is_socket_ = S_ISSOCK(fd_stat.st_mode);
  }

  if (is_socket_) {
    // 必须在 FdManager 持锁期间绕过 hook::fcntl，否则 get() 会重入死锁。
    int flags = static_cast<int>(syscall(SYS_fcntl, fd_, F_GETFL));
    if (!(flags & O_NONBLOCK)) {
      syscall(SYS_fcntl, fd_, F_SETFL, flags | O_NONBLOCK);
    }
    sys_nonblock_ = true;
  } else {
    sys_nonblock_ = false;
  }

  user_nonblock_ = false;
  is_closed_ = false;
  return is_init_;
}

void FdCtx::setTimeout(int type, uint64_t v) {
  if (type == SO_RCVTIMEO) {
    recv_timeout_ = v;
  } else {
    send_timeout_ = v;
  }
}

uint64_t FdCtx::getTimeout(int type) const {
  if (type == SO_RCVTIMEO) {
    return recv_timeout_;
  }
  return send_timeout_;
}

FdManager::FdManager() { datas_.resize(64); }

FdCtx::ptr FdManager::get(int fd, bool auto_create) {
  if (fd == -1) {
    return nullptr;
  }

  RWMutexType::ReadLock lock(mutex_);
  if (static_cast<int>(datas_.size()) > fd) {
    if (datas_[fd] || !auto_create) {
      return datas_[fd];
    }
  }
  lock.unlock();

  RWMutexType::WriteLock lock2(mutex_);
  FdCtx::ptr ctx(new FdCtx(fd));
  if (fd >= static_cast<int>(datas_.size())) {
    datas_.resize(fd * 1.5);
  }
  datas_[fd] = ctx;
  return ctx;
}

void FdManager::del(int fd) {
  RWMutexType::WriteLock lock(mutex_);
  if (static_cast<int>(datas_.size()) <= fd) {
    return;
  }
  datas_[fd].reset();
}

}  // namespace net
