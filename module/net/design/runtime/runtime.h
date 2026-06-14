#ifndef NET_DESIGN_RUNTIME_RUNTIME_H
#define NET_DESIGN_RUNTIME_RUNTIME_H

#include "runtime/scheduler.h"

#include <memory>
#include <string>

namespace net {

class Reactor;

/**
 * @brief 运行时门面：组装 Scheduler + Reactor，替代旧 IOManager。
 */
class Runtime {
 public:
  using ptr = std::shared_ptr<Runtime>;

  Runtime(size_t threads = 1, bool use_caller = true,
          const std::string& name = "");
  ~Runtime();

  void start();
  void stop();

  Scheduler& scheduler() { return *scheduler_; }
  Reactor& reactor() { return *reactor_; }

  static Runtime* GetThis();

 private:
  std::unique_ptr<Scheduler> scheduler_;
  std::unique_ptr<Reactor> reactor_;
};

}  // namespace net

#endif  // NET_DESIGN_RUNTIME_RUNTIME_H
