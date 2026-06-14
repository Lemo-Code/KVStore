#pragma once

#include "lemo/fiber/fiber.h"
#include "lemo/nettycore_export.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace lemo {
namespace fiber {
namespace sync {

LEMO_NETTYCORE_API size_t FiberLocalAllocKey();
LEMO_NETTYCORE_API void FiberLocalSet(uint64_t fiber_id, size_t key,
                                      std::shared_ptr<void> value);
LEMO_NETTYCORE_API std::shared_ptr<void> FiberLocalGet(uint64_t fiber_id,
                                                         size_t key);
LEMO_NETTYCORE_API void FiberLocalEraseFiber(uint64_t fiber_id);

/**
 * @brief 协程局部存储，协程退出时由 Fiber 析构自动清理。
 */
template <typename T>
class FiberLocal {
 public:
  FiberLocal() : key_(FiberLocalAllocKey()) {}

  T& get() {
    const uint64_t fid = Fiber::GetThis()->getId();
    std::shared_ptr<void> p = FiberLocalGet(fid, key_);
    if (!p) {
      std::shared_ptr<T> v(new T());
      FiberLocalSet(fid, key_, std::static_pointer_cast<void>(v));
      return *v;
    }
    return *std::static_pointer_cast<T>(p);
  }

  void reset() {
    const uint64_t fid = Fiber::GetThis()->getId();
    FiberLocalSet(fid, key_, nullptr);
  }

 private:
  size_t key_;
};

}  // namespace sync
}  // namespace fiber
}  // namespace lemo
