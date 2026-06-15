#pragma once

#include <cstdint>
#include <cstdlib>
#include <string>
#include <lstl/container/vector.h>
#include <chrono>

namespace ledis {

class Dict;
struct Value;

// ============================================================
// EvictionManager — 内存淘汰 (maxmemory + LRU/LFU)
// ============================================================
enum class EvictPolicy : uint8_t {
    NOEVICTION,
    ALLKEYS_LRU,
    VOLATILE_LRU,
    ALLKEYS_RANDOM,
    VOLATILE_RANDOM,
    ALLKEYS_LFU,
    VOLATILE_LFU,
};

inline EvictPolicy parseEvictPolicy(const std::string& s) {
    if (s == "allkeys-lru")     return EvictPolicy::ALLKEYS_LRU;
    if (s == "volatile-lru")    return EvictPolicy::VOLATILE_LRU;
    if (s == "allkeys-random")  return EvictPolicy::ALLKEYS_RANDOM;
    if (s == "volatile-random") return EvictPolicy::VOLATILE_RANDOM;
    if (s == "allkeys-lfu")     return EvictPolicy::ALLKEYS_LFU;
    if (s == "volatile-lfu")    return EvictPolicy::VOLATILE_LFU;
    return EvictPolicy::NOEVICTION;
}

class EvictionManager {
public:
    EvictionManager() = default;

    void setPolicy(EvictPolicy p)  { policy_ = p; }
    void setMaxMemory(size_t bytes) { maxmemory_ = bytes; }
    EvictPolicy policy() const { return policy_; }
    size_t maxmemory() const { return maxmemory_; }
    bool enabled() const { return maxmemory_ > 0 && policy_ != EvictPolicy::NOEVICTION; }

    // 估算内存使用 (简化: key+value 大小 + 固定开销)
    static size_t estimateKeySize(const std::string& key, const Value& val);

    // 步骤 1: 写操作前检查，必要时淘汰
    void evictIfNeeded(Dict& dict);

    // 步骤 2: 更新 key 的 LRU/LFU 信息 (每次访问时调用)
    void touchKey(Value& val);

    // 步骤 3: 淘汰一个 key，返回是否成功
    bool evictOne(Dict& dict);

private:
    // 获取全局 LRU 时钟 (每 100ms 约递增 1)
    uint32_t lruClock() const {
        auto now = std::chrono::steady_clock::now().time_since_epoch();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
        return static_cast<uint32_t>(ms / 100);  // 每 100ms 一个 tick
    }

    // 计算 idle 时间 (LRU) 或频率 (LFU)
    int64_t estimateIdle(const Value& val, uint32_t now_clock) const;
    uint8_t estimateFreq(const Value& val) const;

    // 采样候选淘汰 key
    struct Candidate {
        std::string key;
        int64_t     idle_or_freq;  // LRU: idle 越大越优先, LFU: freq 越小越优先
    };

    EvictPolicy policy_ = EvictPolicy::NOEVICTION;
    size_t maxmemory_ = 0;

    // 候选淘汰池
    static constexpr int EVPOOL_SIZE = 16;
    Candidate pool_[EVPOOL_SIZE];
    int pool_size_ = 0;

    // 估算总内存
    size_t estimateTotalMemory(Dict& dict);
};

// ============================================================
// 实现
// ============================================================

inline size_t EvictionManager::estimateKeySize(const std::string& key, const Value& val) {
    size_t s = key.size() + val.str.size();
    s += sizeof(void*) * 4;  // Dict Entry 指针开销
    s += 32;                 // Entry 固定开销
    if (val.opaque_ptr) s += 64;  // 复杂类型粗估
    return s;
}

inline void EvictionManager::touchKey(Value& val) {
    val.lru = lruClock();
}

inline int64_t EvictionManager::estimateIdle(const Value& val, uint32_t now_clock) const {
    uint32_t t = val.lru;
    if (t <= now_clock) return now_clock - t;
    return (int64_t)(now_clock + (0xFFFFFF - t));
}

inline uint8_t EvictionManager::estimateFreq(const Value& val) const {
    return val.lfu;
}

inline size_t EvictionManager::estimateTotalMemory(Dict& dict) {
    size_t total = 0;
    // 简化: 只统计 key+value，跳过迭代开销
    // 生产代码应遍历 dict (使用 Iterator) 累计
    return total;
}

inline void EvictionManager::evictIfNeeded(Dict& dict) {
    if (!enabled()) return;

    // 采样估算总内存 (每 16 次淘汰检查才做一次全量估算)
    static int check_counter = 0;
    if (++check_counter % 16 == 0) {
        size_t used = estimateTotalMemory(dict);
        if (used < maxmemory_) return;
    }

    // 执行淘汰
    size_t dict_size = dict.size();
    size_t target = dict_size > 0 ? dict_size * 9 / 10 : 0;  // 淘汰 10%

    int max_attempts = 64;
    while (max_attempts-- > 0 && dict.size() > target) {
        if (!evictOne(dict)) break;
    }
}

inline bool EvictionManager::evictOne(Dict& dict) {
    if (dict.size() == 0) return false;

    uint32_t now = lruClock();
    bool use_lru = (policy_ == EvictPolicy::ALLKEYS_LRU ||
                    policy_ == EvictPolicy::VOLATILE_LRU);
    bool use_lfu = (policy_ == EvictPolicy::ALLKEYS_LFU ||
                    policy_ == EvictPolicy::VOLATILE_LFU);
    bool volatile_only = (policy_ == EvictPolicy::VOLATILE_LRU ||
                          policy_ == EvictPolicy::VOLATILE_RANDOM ||
                          policy_ == EvictPolicy::VOLATILE_LFU);
    bool random_pick = (policy_ == EvictPolicy::ALLKEYS_RANDOM ||
                        policy_ == EvictPolicy::VOLATILE_RANDOM);

    // 采样 N 个 key
    static constexpr int MAX_SAMPLES = 16;
    Candidate candidates[MAX_SAMPLES];
    int cand_count = 0;
    uint64_t now_ms = static_cast<uint64_t>(now) * 100;

    for (int i = 0; i < MAX_SAMPLES; ++i) {
        Dict::Entry* entry = dict.randomEntry();
        if (!entry) break;

        // 跳过已过期的 (过期的优先删除)
        if (entry->value.expire_at_ms > 0 && now_ms >= entry->value.expire_at_ms) {
            dict.remove(entry->key);
            return true;  // 删了一个过期 key，成功
        }

        // volatile-only 跳过无过期时间的
        if (volatile_only && entry->value.expire_at_ms == 0) continue;

        candidates[cand_count].key = entry->key;
        if (use_lru) {
            candidates[cand_count].idle_or_freq = estimateIdle(entry->value, now);
        } else if (use_lfu) {
            candidates[cand_count].idle_or_freq = estimateFreq(entry->value);
        }
        cand_count++;
    }

    if (cand_count == 0) return false;

    // 选择最优淘汰目标
    int best = 0;
    for (int i = 1; i < cand_count; ++i) {
        bool better = false;
        if (random_pick) {
            better = (rand() % 2) == 0;
        } else if (use_lru) {
            better = candidates[i].idle_or_freq > candidates[best].idle_or_freq;
        } else if (use_lfu) {
            better = candidates[i].idle_or_freq < candidates[best].idle_or_freq;
        }
        if (better) best = i;
    }

    dict.remove(candidates[best].key);
    return true;
}

} // namespace ledis
