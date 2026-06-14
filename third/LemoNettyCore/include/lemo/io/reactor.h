#pragma once

#include "lemo/thread/mutex.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <sys/epoll.h>
#include <vector>

namespace lemo {
namespace fiber {
class Fiber;
class Scheduler;
}  // namespace fiber

namespace io {

/**
 * @brief epoll 反应堆：管理 fd 事件，不继承 Scheduler。
 */
class Reactor {
 public:
  enum Event {
    NONE = 0x0,
    READ = 0x1,
    WRITE = 0x4,
  };

  explicit Reactor(fiber::Scheduler* owner);
  ~Reactor();

  Reactor(const Reactor&) = delete;
  Reactor& operator=(const Reactor&) = delete;

  int addEvent(int fd, Event event, std::function<void()> cb = nullptr);
  bool delEvent(int fd, Event event);
  bool cancelEvent(int fd, Event event);
  bool cancelAll(int fd);

  /** epoll_wait + 分发就绪事件；timeout_ms 会被截断到 100ms 上限 */
  void poll(uint64_t timeout_ms);

  void tickle();
  void cancelAllEvents();

  size_t pendingEventCount() const {
    return pending_event_count_.load(std::memory_order_relaxed);
  }

  struct Stats {
    uint64_t epoll_ctl_ops = 0;
    uint64_t runnext_wakes = 0;
    uint64_t schedule_wakes = 0;
  };

  Stats stats() const;

  void recordEpollCtl();
  void recordRunnextWake();
  void recordScheduleWake();

 private:
  struct FdContext;

  void contextResize(size_t size);

  fiber::Scheduler* owner_;
  int epfd_;
  int ticklefds_[2];
  std::atomic<size_t> pending_event_count_;
  std::atomic<uint64_t> epoll_ctl_ops_{0};
  std::atomic<uint64_t> runnext_wakes_{0};
  std::atomic<uint64_t> schedule_wakes_{0};
  thread::RWMutex mutex_;
  std::vector<FdContext*> fd_contexts_;
};

}  // namespace io
}  // namespace lemo
