#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <lstl/container/vector.h>

#include "kv_ledis/core/value.h"

namespace ledis {

// ============================================================
// Dict — 开放寻址哈希表 (线性探测 + hash 缓存)
// ============================================================
//
// 设计要点:
//   - 槽内缓存 64-bit hash: 探测时先比 hash (8 字节整数), 命中才比 key (字符串)
//     大幅减少探测失败时的字符串比较开销
//   - 50% 负载因子: 平均探测长度 ~1.5, 降低 cache miss
//   - Power-of-2 容量 + 位掩码: 免除法指令
//   - 批量 resize: 触发时一次性迁移, 无渐进 rehash 的每操作开销
//
class Dict {
public:
    static constexpr size_t INITIAL_SIZE = 1024;
    static constexpr double MAX_LOAD     = 0.70;  // 70% 负载均衡内存与探测长度

    enum State : uint8_t { EMPTY = 0, OCCUPIED = 1, DELETED = 2 };

    struct Slot {
        uint64_t    hash = 0;   // 缓存 hash, 0 表示未使用
        std::string key;
        Value       value;
        uint8_t     state = EMPTY;
    };

    class Iterator {
    public:
        explicit Iterator(Dict* dict) : dict_(dict) {
            if (dict_->slots_)
                while (idx_ < dict_->capacity_ &&
                       dict_->slots_[idx_].state != OCCUPIED) ++idx_;
        }
        bool   valid() const { return dict_->slots_ && idx_ < dict_->capacity_
                                     && dict_->slots_[idx_].state == OCCUPIED; }
        void   next() { ++idx_; while (idx_ < dict_->capacity_
                         && dict_->slots_[idx_].state != OCCUPIED) ++idx_; }
        const std::string& key()   const { return dict_->slots_[idx_].key; }
        Value&             value() const { return dict_->slots_[idx_].value; }
    private:
        Dict* dict_;
        size_t idx_ = 0;
    };

    Dict();
    ~Dict();

    Value* find(const std::string& key);
    Value* insert(std::string key, Value value);
    Value  remove(const std::string& key);

    size_t size()     const { return used_; }
    size_t capacity() const { return capacity_; }
    Slot*  randomSlot();

    // FNV-1a: 稳定高效, 避免 CRC32 unaligned access 开销
    static inline uint64_t hashKey(const std::string& key) {
        uint64_t h = 14695981039346656037ULL;
        for (char c : key) {
            h ^= static_cast<uint8_t>(c);
            h *= 1099511628211ULL;
        }
        return h ? h : 1;
    }

private:
    void resize(size_t new_capacity);

    Slot*  slots_    = nullptr;
    size_t capacity_ = 0;
    size_t mask_     = 0;
    size_t used_     = 0;
    size_t deleted_  = 0;
};

// ============================================================
// 实现
// ============================================================

inline Dict::Dict() {}
inline Dict::~Dict() { delete[] slots_; }

inline Value* Dict::find(const std::string& key) {
    if (!slots_) return nullptr;

    uint64_t h = hashKey(key);
    size_t idx = h & mask_;

    while (true) {
        uint8_t s = slots_[idx].state;
        if (s == EMPTY) return nullptr;
        if (s == OCCUPIED && slots_[idx].hash == h && slots_[idx].key == key)
            return &slots_[idx].value;
        idx = (idx + 1) & mask_;
    }
}

inline Value* Dict::insert(std::string key, Value value) {
    if (!slots_) resize(INITIAL_SIZE);

    uint64_t h = hashKey(key);

    // 扩容检查: 用 (used + deleted) / capacity
    if (static_cast<double>(used_ + deleted_ + 1) / capacity_ > MAX_LOAD)
        resize(capacity_ * 2);

    // 查找: 是否已存在 / 找到插入位置
    size_t idx = h & mask_;
    size_t first_del = size_t(-1);

    while (true) {
        uint8_t s = slots_[idx].state;

        if (s == OCCUPIED) {
            // 先比 hash (快速), 命中才比 key
            if (slots_[idx].hash == h && slots_[idx].key == key) {
                slots_[idx].value = std::move(value);
                return &slots_[idx].value;
            }
        } else {
            if (s == EMPTY) {
                if (first_del == size_t(-1)) first_del = idx;
                break;
            }
            if (s == DELETED && first_del == size_t(-1))
                first_del = idx;
        }
        idx = (idx + 1) & mask_;
    }

    // 插入
    if (first_del == size_t(-1)) return nullptr;  // 不应发生
    size_t slot = first_del;
    if (slots_[slot].state == DELETED) deleted_--;
    slots_[slot].hash  = h;
    slots_[slot].key   = std::move(key);
    slots_[slot].value = std::move(value);
    slots_[slot].state = OCCUPIED;
    used_++;
    return &slots_[slot].value;
}

inline Value Dict::remove(const std::string& key) {
    if (!slots_) return Value{};

    uint64_t h = hashKey(key);
    size_t idx = h & mask_;

    while (true) {
        uint8_t s = slots_[idx].state;
        if (s == EMPTY) return Value{};
        if (s == OCCUPIED && slots_[idx].hash == h && slots_[idx].key == key) {
            slots_[idx].state = DELETED;
            deleted_++;
            used_--;
            return std::move(slots_[idx].value);
        }
        idx = (idx + 1) & mask_;
    }
}

inline Dict::Slot* Dict::randomSlot() {
    if (!slots_ || used_ == 0) return nullptr;
    for (int attempt = 0; attempt < 200; ++attempt) {
        size_t idx = static_cast<size_t>(rand()) & mask_;
        if (slots_[idx].state == OCCUPIED) return &slots_[idx];
    }
    for (size_t i = 0; i < capacity_; ++i)
        if (slots_[i].state == OCCUPIED) return &slots_[i];
    return nullptr;
}

inline void Dict::resize(size_t new_capacity) {
    Slot* new_slots = new Slot[new_capacity]();
    size_t new_mask = new_capacity - 1;

    if (slots_) {
        for (size_t i = 0; i < capacity_; ++i) {
            if (slots_[i].state != OCCUPIED) continue;

            uint64_t h = slots_[i].hash;
            size_t idx = h & new_mask;
            while (new_slots[idx].state == OCCUPIED)
                idx = (idx + 1) & new_mask;

            new_slots[idx].hash  = h;
            new_slots[idx].key   = std::move(slots_[i].key);
            new_slots[idx].value = std::move(slots_[i].value);
            new_slots[idx].state = OCCUPIED;
        }
        delete[] slots_;
    }

    slots_    = new_slots;
    capacity_ = new_capacity;
    mask_     = new_mask;
    deleted_  = 0;
}

} // namespace ledis
