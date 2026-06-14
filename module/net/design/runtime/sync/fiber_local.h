#ifndef NET_DESIGN_RUNTIME_SYNC_FIBER_LOCAL_H
#define NET_DESIGN_RUNTIME_SYNC_FIBER_LOCAL_H

namespace net {
namespace sync {

/**
 * @brief 协程局部存储：协程退出时自动析构。
 */
template <typename T>
class FiberLocal {
 public:
  T& get();
  void reset();
};

}  // namespace sync
}  // namespace net

#endif  // NET_DESIGN_RUNTIME_SYNC_FIBER_LOCAL_H
