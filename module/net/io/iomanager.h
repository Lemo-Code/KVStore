#ifndef NET_IO_IOMANAGER_H
#define NET_IO_IOMANAGER_H

#include "fiber/scheduler.h"
#include "thread/mutex.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <sys/epoll.h>
#include <vector>

namespace net {

/**
 * @brief IO 调度器：epoll + pipe tickle，继承 Scheduler（含定时器）。
 */
class IOManager : public Scheduler {
 public:
  typedef std::shared_ptr<IOManager> ptr;
  typedef RWMutex RWMutexType;

  enum Event {
    NONE = 0x0,
    READ = 0x1,
    WRITE = 0x4,
  };

  IOManager(size_t threads = 1, bool use_caller = true,
            const std::string& name = "");
  ~IOManager() override;

  void stop() override;

  int addEvent(int fd, Event event, std::function<void()> cb = nullptr);
  bool delEvent(int fd, Event event);
  bool cancelEvent(int fd, Event event);
  bool cancelAll(int fd);

  static IOManager* GetThis();

 protected:
  void tickle() override;
  bool stopping() override;
  void onTimerInsertedAtFront() override;
  void idle() override;
  void run() override;

  void contextResize(size_t size);
  bool stopping(uint64_t& timeout);

 private:
  struct FdContext;

  FdContext* getFdContext(int fd);
  void cancelAllEvents();

  int epfd_ = 0;
  int ticklefds_[2] = {-1, -1};
  std::atomic<size_t> pending_event_count_{0};
  RWMutexType mutex_;
  std::vector<FdContext*> fd_contexts_;
};

}  // namespace net

#endif  // NET_IO_IOMANAGER_H
