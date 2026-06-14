#include "lemo/fiber/sync/fiber_local.h"

#include "lemo/thread/mutex.h"

#include <atomic>
#include <memory>
#include <unordered_map>

namespace lemo {
namespace fiber {
namespace sync {

namespace {

std::atomic<size_t> s_next_key{0};
std::unordered_map<uint64_t, std::unordered_map<size_t, std::shared_ptr<void>>>
    s_data;
thread::Mutex s_mutex;

}  // namespace

size_t FiberLocalAllocKey() {
  return s_next_key.fetch_add(1, std::memory_order_relaxed);
}

void FiberLocalSet(uint64_t fiber_id, size_t key, std::shared_ptr<void> value) {
  thread::Mutex::Lock lock(s_mutex);
  if (value) {
    s_data[fiber_id][key] = std::move(value);
    return;
  }
  auto it = s_data.find(fiber_id);
  if (it == s_data.end()) {
    return;
  }
  it->second.erase(key);
  if (it->second.empty()) {
    s_data.erase(it);
  }
}

std::shared_ptr<void> FiberLocalGet(uint64_t fiber_id, size_t key) {
  thread::Mutex::Lock lock(s_mutex);
  auto it = s_data.find(fiber_id);
  if (it == s_data.end()) {
    return nullptr;
  }
  auto it2 = it->second.find(key);
  if (it2 == it->second.end()) {
    return nullptr;
  }
  return it2->second;
}

void FiberLocalEraseFiber(uint64_t fiber_id) {
  thread::Mutex::Lock lock(s_mutex);
  s_data.erase(fiber_id);
}

}  // namespace sync
}  // namespace fiber
}  // namespace lemo
