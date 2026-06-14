#pragma once

/**
 * @file epoll_context.h
 * @brief IOManager 内部 epoll 实现（独立编译单元，不对外暴露）。
 */

#include "lemo/thread/mutex.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <sys/epoll.h>
#include <vector>

namespace lemo {
namespace fiber {
class Scheduler;
}  // namespace fiber

namespace io {

class EpollContext {
 public:
  enum Event {
    NONE = 0x0,
    READ = 0x1,
    WRITE = 0x4,
  };

  explicit EpollContext(fiber::Scheduler* owner);
  ~EpollContext();

  EpollContext(const EpollContext&) = delete;
  EpollContext& operator=(const EpollContext&) = delete;

  int addEvent(int fd, Event event, std::function<void()> cb = nullptr);
  bool delEvent(int fd, Event event);
  bool cancelEvent(int fd, Event event);
  bool cancelAll(int fd);

  void poll(uint64_t timeout_ms);
  void tickle();
  void cancelAllEvents();

  size_t pendingEventCount() const {
    return pending_event_count_.load(std::memory_order_relaxed);
  }

 private:
  struct FdContext;

  void contextResize(size_t size);

  fiber::Scheduler* owner_;
  int epfd_;
  int ticklefds_[2];
  std::atomic<size_t> pending_event_count_;
  thread::RWMutex mutex_;
  std::vector<FdContext*> fd_contexts_;
};

}  // namespace io
}  // namespace lemo
