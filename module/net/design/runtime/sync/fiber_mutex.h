#ifndef NET_DESIGN_RUNTIME_SYNC_FIBER_MUTEX_H
#define NET_DESIGN_RUNTIME_SYNC_FIBER_MUTEX_H

namespace net {
namespace sync {

class FiberMutex {
 public:
  void lock();
  void unlock();
  bool try_lock();
};

}  // namespace sync
}  // namespace net

#endif  // NET_DESIGN_RUNTIME_SYNC_FIBER_MUTEX_H
