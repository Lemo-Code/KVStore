#ifndef NET_DESIGN_IO_REACTOR_H
#define NET_DESIGN_IO_REACTOR_H

#include <cstdint>
#include <functional>

namespace net {

class Fiber;

/**
 * @brief epoll 反应堆：管理 fd 事件，不继承 Scheduler。
 */
class Reactor {
 public:
  enum Event : uint32_t {
    NONE = 0,
    READ = 1,
    WRITE = 4,
  };

  explicit Reactor(class Scheduler* owner);
  ~Reactor();

  int addEvent(int fd, Event ev, std::function<void()> cb = nullptr);
  bool delEvent(int fd, Event ev);
  bool cancelEvent(int fd, Event ev);
  bool cancelAll(int fd);

  /** @return 就绪事件数 */
  int poll(uint64_t timeout_ms);

  void tickle();

 private:
  Scheduler* owner_ = nullptr;
};

}  // namespace net

#endif  // NET_DESIGN_IO_REACTOR_H
