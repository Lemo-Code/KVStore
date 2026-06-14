#include "sylar/fiber_local.h"
#include "sylar/mutex.h"
#include <unordered_map>
#include <atomic>

namespace sylar {

static std::atomic<size_t> s_nextKey(0);
static std::unordered_map<uint64_t, std::unordered_map<size_t, std::shared_ptr<void>>> s_data;
static sylar::Mutex s_mutex;

size_t FiberLocalAllocKey() {
    return s_nextKey++;
}

void FiberLocalSet(uint64_t fiber_id, size_t key, std::shared_ptr<void> value) {
    sylar::Mutex::Lock lock(s_mutex);
    if (value) {
        s_data[fiber_id][key] = std::move(value);
    } else {
        auto it = s_data.find(fiber_id);
        if (it != s_data.end()) {
            it->second.erase(key);
            if (it->second.empty()) s_data.erase(it);
        }
    }
}

std::shared_ptr<void> FiberLocalGet(uint64_t fiber_id, size_t key) {
    sylar::Mutex::Lock lock(s_mutex);
    auto it = s_data.find(fiber_id);
    if (it == s_data.end()) return nullptr;
    auto it2 = it->second.find(key);
    if (it2 == it->second.end()) return nullptr;
    return it2->second;
}

void FiberLocalEraseFiber(uint64_t fiber_id) {
    sylar::Mutex::Lock lock(s_mutex);
    s_data.erase(fiber_id);
}

} // namespace sylar
