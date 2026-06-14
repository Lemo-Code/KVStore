#pragma once

#include "lemo/fiber/scheduler.h"
#include "lemo/thread/mutex.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <sys/epoll.h>
#include <vector>

namespace lemo {
namespace io {

/**
 * @brief IO 调度器（sylar 风格）：Scheduler + epoll + pipe tickle，无独立 Reactor 类。
 */
class IOManager : public fiber::Scheduler {
 public:
  typedef std::shared_ptr<IOManager> ptr;

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

  size_t pendingEventCount() const {
    return pending_event_count_.load(std::memory_order_relaxed);
  }

  static IOManager* GetThis();

 protected:
  void run() override;
  void tickle() override;
  bool stopping() override;
  void onTimerInsertedAtFront() override;
  void idle() override;
  fiber::Fiber::ptr newIdleFiber() override {
    return fiber::Fiber::ptr(new fiber::Fiber([this]() { idle(); }));
  }

  bool stopping(uint64_t& timeout);

 private:
  struct FdContext;

  void initEpoll();
  void contextResize(size_t size);
  void cancelAllEvents();
  void pollEvents(uint64_t timeout_ms);

  fiber::Scheduler* epoll_owner_ = nullptr;
  int epfd_ = 0;
  int ticklefds_[2] = {-1, -1};
  std::atomic<size_t> pending_event_count_{0};
  thread::RWMutex mutex_;
  std::vector<FdContext*> fd_contexts_;
};

}  // namespace io
}  // namespace lemo
