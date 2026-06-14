#pragma once

#include "lemo/thread/lock_types.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace lemo {
namespace io {

class FdContext {
 public:
  typedef std::shared_ptr<FdContext> ptr;

  explicit FdContext(int fd);
  ~FdContext();

  bool init();
  bool isInit() const { return is_init_; }
  bool isSocket() const { return is_socket_; }
  bool isClose() const { return is_closed_; }
  void setClose() { is_closed_ = true; }

  void setUserNonBlock(bool v) { user_nonblock_ = v; }
  bool getUserNonBlock() const { return user_nonblock_; }

  void setSysNonBlock(bool v) { sys_nonblock_ = v; }
  bool getSysNonBlock() const { return sys_nonblock_; }

  void setTimeout(int type, uint64_t v);
  uint64_t getTimeout(int type) const;

 private:
  bool is_init_ : 1;
  bool is_socket_ : 1;
  bool sys_nonblock_ : 1;
  bool user_nonblock_ : 1;
  bool is_closed_ : 1;
  int fd_;
  uint64_t recv_timeout_;
  uint64_t send_timeout_;
};

class FdManager {
 public:
  using MutexType = thread::RegistryMutex;

  static FdManager& Instance();

  FdContext::ptr get(int fd, bool auto_create = false);
  void del(int fd);

 private:
  FdManager();

  MutexType mutex_;
  std::vector<FdContext::ptr> datas_;
};

}  // namespace io
}  // namespace lemo
