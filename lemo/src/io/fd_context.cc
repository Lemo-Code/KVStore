#include "lemo/io/fd_context.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef SYS_fcntl
#define SYS_fcntl __NR_fcntl
#endif

namespace lemo {
namespace io {

FdContext::FdContext(int fd)
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

FdContext::~FdContext() {}

bool FdContext::init() {
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
    const int flags = static_cast<int>(syscall(SYS_fcntl, fd_, F_GETFL));
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

void FdContext::setTimeout(int type, uint64_t v) {
  if (type == SO_RCVTIMEO) {
    recv_timeout_ = v;
  } else {
    send_timeout_ = v;
  }
}

uint64_t FdContext::getTimeout(int type) const {
  if (type == SO_RCVTIMEO) {
    return recv_timeout_;
  }
  return send_timeout_;
}

FdManager::FdManager() { datas_.resize(64); }

FdManager& FdManager::Instance() {
  static FdManager mgr;
  return mgr;
}

FdContext::ptr FdManager::get(int fd, bool auto_create) {
  if (fd == -1) {
    return FdContext::ptr();
  }

  FdContext::ptr cached;
  {
    MutexType::ReadLock lock(mutex_);
    if (static_cast<int>(datas_.size()) > fd) {
      cached = datas_[fd];
    }
  }

  if (cached) {
    if (!auto_create) {
      return cached;
    }
    if (cached->isClose() || ::fcntl(fd, F_GETFD) < 0) {
      MutexType::WriteLock wlock(mutex_);
      if (static_cast<int>(datas_.size()) > fd && datas_[fd] == cached) {
        datas_[fd].reset();
      }
      cached.reset();
    } else {
      return cached;
    }
  }

  if (!auto_create) {
    return FdContext::ptr();
  }

  MutexType::WriteLock lock(mutex_);
  if (static_cast<int>(datas_.size()) > fd && datas_[fd]) {
    return datas_[fd];
  }
  FdContext::ptr ctx(new FdContext(fd));
  if (fd >= static_cast<int>(datas_.size())) {
    datas_.resize(static_cast<size_t>(fd * 1.5));
  }
  datas_[fd] = ctx;
  return ctx;
}

void FdManager::del(int fd) {
  MutexType::WriteLock lock(mutex_);
  if (static_cast<int>(datas_.size()) <= fd) {
    return;
  }
  datas_[fd].reset();
}

}  // namespace io
}  // namespace lemo
