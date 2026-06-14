#pragma once

#include "lemo/fiber/scheduler.h"
#include "lemo/io/reactor.h"

#include <memory>
#include <string>

namespace lemo {
namespace io {

/**
 * @brief IO 调度器：fiber::Scheduler + Reactor（epoll + pipe tickle）。
 */
class IOManager : public fiber::Scheduler {
 public:
  typedef std::shared_ptr<IOManager> ptr;
  typedef Reactor::Event Event;
  static const Event NONE = Reactor::NONE;
  static const Event READ = Reactor::READ;
  static const Event WRITE = Reactor::WRITE;

  IOManager(size_t threads = 1, bool use_caller = true,
            const std::string& name = "");
  ~IOManager() override;

  void stop() override;

  int addEvent(int fd, Event event, std::function<void()> cb = nullptr) {
    return reactor_.addEvent(fd, event, std::move(cb));
  }
  bool delEvent(int fd, Event event) { return reactor_.delEvent(fd, event); }
  bool cancelEvent(int fd, Event event) {
    return reactor_.cancelEvent(fd, event);
  }
  bool cancelAll(int fd);

  Reactor& reactor() { return reactor_; }
  const Reactor& reactor() const { return reactor_; }

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
  Reactor reactor_;
};

}  // namespace io
}  // namespace lemo
