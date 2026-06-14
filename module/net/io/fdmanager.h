#ifndef NET_IO_FDMANAGER_H
#define NET_IO_FDMANAGER_H

#include "common/singleton.h"
#include "thread/mutex.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace net {

/**
 * @brief 文件描述符上下文：识别 socket、非阻塞状态、超时等。
 */
class FdCtx : public std::enable_shared_from_this<FdCtx> {
 public:
  typedef std::shared_ptr<FdCtx> ptr;

  explicit FdCtx(int fd);
  ~FdCtx();

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

/**
 * @brief 全局 fd 管理器，按 fd 索引存储 FdCtx。
 */
class FdManager {
 public:
  typedef RWMutex RWMutexType;

  FdManager();

  FdCtx::ptr get(int fd, bool auto_create = false);
  void del(int fd);

 private:
  RWMutexType mutex_;
  std::vector<FdCtx::ptr> datas_;
};

typedef Singleton<FdManager> FdMgr;

}  // namespace net

#endif  // NET_IO_FDMANAGER_H
