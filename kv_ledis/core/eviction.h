#pragma once

// ============================================================
// EvictionManager — maxmemory + 淘汰策略
// ============================================================
// LRU: 24-bit 时钟, 每次访问更新 lru 字段
// LFU: 8-bit 对数计数器, 随时间衰减
//
// 策略:
//   noeviction       — 拒绝写入, 返回错误
//   allkeys-lru      — 全键 LRU
//   allkeys-lfu      — 全键 LFU
//   allkeys-random   — 全键随机
//   volatile-lru     — 有过期时间的键 LRU
//   volatile-lfu     — 有过期时间的键 LFU
//   volatile-random  — 有过期时间的键随机
//   volatile-ttl     — 最接近过期时间的键
//

#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <string>
#include <lstl/container/vector.h>
#include "kv_ledis/core/dict.h"
#include "kv_ledis/core/value.h"

namespace ledis {

enum EvictionPolicy : uint8_t {
    EVICT_NOEVICTION       = 0,
    EVICT_ALLKEYS_LRU      = 1,
    EVICT_ALLKEYS_LFU      = 2,
    EVICT_ALLKEYS_RANDOM   = 3,
    EVICT_VOLATILE_LRU     = 4,
    EVICT_VOLATILE_LFU     = 5,
    EVICT_VOLATILE_RANDOM  = 6,
    EVICT_VOLATILE_TTL     = 7,
};

inline const char* evictionPolicyName(EvictionPolicy p) {
    switch (p) {
    case EVICT_NOEVICTION:       return "noeviction";
    case EVICT_ALLKEYS_LRU:      return "allkeys-lru";
    case EVICT_ALLKEYS_LFU:      return "allkeys-lfu";
    case EVICT_ALLKEYS_RANDOM:   return "allkeys-random";
    case EVICT_VOLATILE_LRU:     return "volatile-lru";
    case EVICT_VOLATILE_LFU:     return "volatile-lfu";
    case EVICT_VOLATILE_RANDOM:  return "volatile-random";
    case EVICT_VOLATILE_TTL:     return "volatile-ttl";
    default: return "unknown";
    }
}

inline EvictionPolicy evictionPolicyFromString(const std::string& s) {
    if (s == "allkeys-lru")      return EVICT_ALLKEYS_LRU;
    if (s == "allkeys-lfu")      return EVICT_ALLKEYS_LFU;
    if (s == "allkeys-random")   return EVICT_ALLKEYS_RANDOM;
    if (s == "volatile-lru")     return EVICT_VOLATILE_LRU;
    if (s == "volatile-lfu")     return EVICT_VOLATILE_LFU;
    if (s == "volatile-random")  return EVICT_VOLATILE_RANDOM;
    if (s == "volatile-ttl")     return EVICT_VOLATILE_TTL;
    return EVICT_NOEVICTION;
}

class EvictionManager {
public:
    EvictionManager() = default;

    void setMaxmemory(size_t bytes) { maxmemory_ = bytes; }
    size_t maxmemory() const { return maxmemory_; }
    void setPolicy(EvictionPolicy p) { policy_ = p; }
    EvictionPolicy policy() const { return policy_; }

    // 获取当前 LRU 时钟 (24bit, ~100ms 精度)
    uint32_t lruClock() const {
        // 用 time() 近似, 约 1 秒精度
        return static_cast<uint32_t>(std::time(nullptr)) & 0xFFFFFF;
    }

    // 更新对象的 LRU 时间戳
    void updateLRU(Value& v) {
        v.lru = lruClock();
    }

    // LFU: 对数递增 + 衰减
    void updateLFU(Value& v) {
        uint32_t now = lruClock();
        uint32_t ldt = v.lru >> 8;   // 最后衰减时间
        uint8_t counter = static_cast<uint8_t>(v.lru & 0xFF);

        // 衰减: 每分钟减 1
        if (now > ldt) {
            uint32_t periods = now - ldt;
            if (periods > 10) periods = 10;
            while (periods-- && counter > 0) counter--;
        }

        // 对数递增
        if (counter < 255) {
            double r = static_cast<double>(rand()) / RAND_MAX;
            double p = 1.0 / (counter * 0.1 + 1);
            if (r < p) counter++;
        }

        v.lru = (static_cast<uint32_t>(now) << 8) | counter;
    }

    // 淘汰一个 key (采样 + 选择)
    // 返回被淘汰的 key 数量
    int evict(Dict& dict, size_t target_bytes) {
        if (maxmemory_ == 0 || policy_ == EVICT_NOEVICTION) return 0;

        // 计算当前内存 (粗略: capacity * sizeof(Slot))
        size_t used = estimateMemory(dict);
        int evicted = 0;

        static constexpr int MAX_SAMPLES = 20;
        static constexpr int MAX_EVICT   = 50;

        while (used > target_bytes && evicted < MAX_EVICT) {
            if (dict.size() == 0) break;

            // 采样
            Dict::Slot* best = nullptr;
            for (int i = 0; i < MAX_SAMPLES; ++i) {
                Dict::Slot* s = dict.randomSlot();
                if (!s || s->state != Dict::OCCUPIED) continue;

                if (!best || isWorse(s->value, best->value)) {
                    best = s;
                }
            }

            if (!best) break;

            dict.remove(best->key);
            evicted++;
            used = estimateMemory(dict);
        }

        return evicted;
    }

    // 检查是否需要淘汰
    size_t estimateMemory(const Dict& dict) const {
        // 粗略估算: capacity * slot_size + used * key_value_overhead
        return dict.capacity() * 96 + dict.size() * 64;  // ~96 bytes/slot
    }

private:
    bool isWorse(const Value& a, const Value& b) {
        switch (policy_) {
        case EVICT_ALLKEYS_LRU:
        case EVICT_VOLATILE_LRU:
            return a.lru < b.lru;  // 更旧 = 更该淘汰

        case EVICT_ALLKEYS_LFU:
        case EVICT_VOLATILE_LFU:
            return (a.lru & 0xFF) < (b.lru & 0xFF);  // 更低频率

        case EVICT_ALLKEYS_RANDOM:
        case EVICT_VOLATILE_RANDOM:
            return false;  // 随机, 第一个采样

        case EVICT_VOLATILE_TTL:
            // 更接近过期 = 更该淘汰
            if (a.expire_at_ms == 0) return false;  // 无过期, 不淘汰
            if (b.expire_at_ms == 0) return true;
            return a.expire_at_ms < b.expire_at_ms;

        default:
            return false;
        }
    }

    size_t maxmemory_ = 0;
    EvictionPolicy policy_ = EVICT_NOEVICTION;
};

} // namespace ledis
