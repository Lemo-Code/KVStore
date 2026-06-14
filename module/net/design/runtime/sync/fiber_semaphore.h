#ifndef NET_DESIGN_RUNTIME_SYNC_FIBER_SEMAPHORE_H
#define NET_DESIGN_RUNTIME_SYNC_FIBER_SEMAPHORE_H

#include <cstdint>

namespace net {
namespace sync {

class FiberSemaphore {
 public:
  explicit FiberSemaphore(uint32_t count = 0);
  void wait();
  void signal();
  void signalN(uint32_t n);
};

}  // namespace sync
}  // namespace net

#endif  // NET_DESIGN_RUNTIME_SYNC_FIBER_SEMAPHORE_H
