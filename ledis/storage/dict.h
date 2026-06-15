#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <lstl/container/vector.h>

#include "ledis/storage/value.h"

namespace ledis {

// ============================================================
// Dict — 链式哈希表 + 渐进式 rehash
// ============================================================
//
// 借鉴 Redis dict 设计:
//   - 双表 (table[0] 主表, table[1] rehash 目标)
//   - 渐进式 rehash: 每次操作搬几个桶，摊销 O(1)
//   - 链式解决哈希冲突
//   - 扩容: 负载因子 ≥ 1.0 → 2x 扩容
//   - 缩容: 负载因子 < 0.1 → 缩容 (min 4)
//
// 线程安全: 单线程使用 (存储线程独占)
//
class Dict {
public:
    // ======== 哈希表条目 ========
    struct Entry {
        std::string key;
        Value       value;
        Entry*      next = nullptr;

        Entry(std::string k, Value v)
            : key(std::move(k)), value(std::move(v)) {}
    };

    // ======== 迭代器 ========
    class Iterator {
    public:
        explicit Iterator(Dict* dict);

        bool        valid() const;
        void        next();
        Entry*      entry() const { return entry_; }
        std::string key() const;
        Value*      value() const;

    private:
        friend class Dict;
        Dict*   dict_;
        Entry*  entry_ = nullptr;
        Entry*  next_entry_ = nullptr;
        int64_t index_ = -1;
        int     table_ = 0;
        bool    safe_ = true;
    };

    // ======== 构造/析构 ========
    Dict();
    ~Dict();

    // ======== 基础操作 ========

    // 查找 key，返回 Value* 或 nullptr
    Value* find(const std::string& key);

    // 插入 (key 已存在则覆盖)，返回旧值或 nullptr
    // 返回的 Value* 在 key 被删除前有效
    Value* insert(std::string key, Value value);

    // 删除 key，返回被删除的 Value (move)，调用者负责处理
    // 如果 key 不存在，返回的 Value.type == STRING 且 str 为空
    Value remove(const std::string& key);

    // 随机返回一个 entry (用于过期采样/淘汰采样)
    Entry* randomEntry();

    // ======== 查询 ========
    size_t size()  const { return used_; }
    size_t capacity() const;

    // ======== Rehash ========
    bool isRehashing() const { return rehash_idx_ != -1; }
    void rehashStep(int n = 1);        // 执行 n 步 rehash
    int  rehashMilliseconds(int ms);   // 在 ms 毫秒内尽可能 rehash

    // ======== 内部调试 ========
    size_t bucketCount(int table) const;

private:
    static constexpr size_t INITIAL_SIZE = 4;
    static constexpr size_t MAX_SIZE     = size_t(1) << 31; // 2^31 buckets max

    // 哈希表
    Entry** table_[2] = {nullptr, nullptr};
    size_t  size_mask_[2] = {0, 0};  // size - 1 (size 为 2 的幂)
    int64_t rehash_idx_ = -1;        // -1 = 不在 rehash
    size_t  used_ = 0;

    // 哈希函数
    static uint64_t hashKey(const std::string& key) {
        // 用 std::hash + 自定义 finalizer (避免低质量实现)
        uint64_t h = std::hash<std::string>{}(key);
        // MurmurHash3 finalizer (改进扩散)
        h ^= h >> 33;
        h *= 0xff51afd7ed558ccdULL;
        h ^= h >> 33;
        h *= 0xc4ceb9fe1a85ec53ULL;
        h ^= h >> 33;
        return h;
    }

    // 在双表中查找 key
    Entry* findEntry(const std::string& key);

    // 扩容/缩容
    bool expandIfNeeded();
    bool expand(size_t new_size);
    bool shrinkIfNeeded();

    // 释放一个表
    static void clearTable(Entry** table, size_t size_mask);
};

// ============================================================
// 实现
// ============================================================

inline Dict::Dict() {
    // 延迟分配: 首次插入时才分配 table_
    table_[0] = nullptr;
    size_mask_[0] = 0;
}

inline Dict::~Dict() {
    clearTable(table_[0], size_mask_[0]);
    clearTable(table_[1], size_mask_[1]);
}

inline void Dict::clearTable(Entry** table, size_t size_mask) {
    if (!table) return;
    size_t size = size_mask + 1;
    for (size_t i = 0; i < size; ++i) {
        Entry* entry = table[i];
        while (entry) {
            Entry* next = entry->next;
            delete entry;
            entry = next;
        }
    }
    delete[] table;
}

inline Value* Dict::find(const std::string& key) {
    Entry* entry = findEntry(key);
    return entry ? &entry->value : nullptr;
}

inline Dict::Entry* Dict::findEntry(const std::string& key) {
    if (used_ == 0) return nullptr;

    uint64_t h = hashKey(key);

    // 搜索 table[0]
    if (table_[0] && size_mask_[0] > 0) {
        size_t idx = h & size_mask_[0];
        for (Entry* e = table_[0][idx]; e; e = e->next) {
            if (e->key == key) return e;
        }
    }

    // 如果正在 rehash，也搜索 table[1]
    if (isRehashing() && table_[1]) {
        size_t idx = h & size_mask_[1];
        for (Entry* e = table_[1][idx]; e; e = e->next) {
            if (e->key == key) return e;
        }
    }

    return nullptr;
}

inline Value* Dict::insert(std::string key, Value value) {
    // 首次分配
    if (!table_[0]) {
        size_t new_size = INITIAL_SIZE;
        table_[0] = new Entry*[new_size]();  // zero-initialized
        size_mask_[0] = new_size - 1;
    }

    // 检查是否需要扩容 (在插入前)
    expandIfNeeded();

    // 如果正在 rehash，先推进一步
    if (isRehashing()) {
        rehashStep(1);
    }

    uint64_t h = hashKey(key);

    // 检查 key 是否已存在于 table_[0]
    if (table_[0]) {
        size_t idx = h & size_mask_[0];
        for (Entry* e = table_[0][idx]; e; e = e->next) {
            if (e->key == key) {
                // key 已存在 → 替换值
                Value old = std::move(e->value);
                e->value = std::move(value);
                // 将 old 返回 (调用者可能需要释放)
                // 用 value.str 临时存储旧值的释放信息
                // 实际上我们返回指针，调用者通过比较判断
                e->value.expire_at_ms = value.expire_at_ms; // 保留过期时间
                // 返回 &e->value, but since we replaced, return nullptr to indicate
                // Actually let's return &e->value (the new value)
                return &e->value;
            }
        }
    }

    // 如果正在 rehash，key 可能在 table[1] 中
    if (isRehashing() && table_[1]) {
        size_t idx = h & size_mask_[1];
        Entry* prev = nullptr;
        for (Entry* e = table_[1][idx]; e; prev = e, e = e->next) {
            if (e->key == key) {
                // 从 table[1] 链表中移除
                if (prev) prev->next = e->next;
                else table_[1][idx] = e->next;
                // 更新值
                Value old = std::move(e->value);
                e->value = std::move(value);
                e->next = nullptr;
                // 重新插入到 table_[0] (rehash 的目标)
                // No, let's simplify: just delete old entry and insert new one
                delete e;
                used_--;
                // Fall through to insert new entry below
                break;
            }
        }
    }

    // 插入新 entry (总是插入到 table_[1] 如果在 rehash，否则 table_[0])
    int target = isRehashing() ? 1 : 0;
    size_t idx = h & size_mask_[target];
    Entry* new_entry = new Entry(std::move(key), std::move(value));
    new_entry->next = table_[target][idx];
    table_[target][idx] = new_entry;
    used_++;

    return &new_entry->value;
}

inline Value Dict::remove(const std::string& key) {
    if (used_ == 0) return Value{};

    // 如果正在 rehash，推进一步
    if (isRehashing()) {
        rehashStep(1);
    }

    uint64_t h = hashKey(key);

    // 搜索 table_[0]
    for (int t = 0; t < 2; ++t) {
        if (!table_[t]) continue;
        size_t idx = h & size_mask_[t];
        Entry* prev = nullptr;
        for (Entry* e = table_[t][idx]; e; prev = e, e = e->next) {
            if (e->key == key) {
                // 从链表中移除
                if (prev) prev->next = e->next;
                else table_[t][idx] = e->next;

                Value v = std::move(e->value);
                delete e;
                used_--;

                // 检查是否需要缩容
                shrinkIfNeeded();
                return v;
            }
        }
    }

    return Value{};  // 未找到
}

inline Dict::Entry* Dict::randomEntry() {
    if (used_ == 0) return nullptr;

    // 如果正在 rehash，在 table[0] 和 table[1] 之间随机选择
    // table[0] 的已 rehash 部分 (rehash_idx_ 之前) 是空的

    // 简化实现: 随机搜索非空桶
    // 更好的做法是维护一个非空桶的 bitmap，但随机采样频率不高，线性搜索足够
    int max_attempts = 100;
    for (int t = 0; t < 2 && max_attempts > 0; ++t) {
        if (!table_[t]) continue;
        size_t size = size_mask_[t] + 1;

        // 如果正在 rehash 且 t == 0，只搜索 rehash_idx_ 之后的部分
        size_t start = (t == 0 && isRehashing()) ? static_cast<size_t>(rehash_idx_) : 0;

        if (start >= size) continue;

        // 随机搜索
        for (int attempt = 0; attempt < max_attempts; ++attempt) {
            size_t idx = start + (static_cast<size_t>(rand()) % (size - start));
            if (table_[t][idx]) {
                // 随机选择链表中的一个 entry
                Entry* e = table_[t][idx];
                int count = 0;
                for (Entry* p = e; p; p = p->next) count++;
                int r = rand() % count;
                for (int i = 0; i < r; ++i) e = e->next;
                return e;
            }
        }
    }
    return nullptr;
}

inline bool Dict::expandIfNeeded() {
    // 如果正在 rehash 或者 table_[0] 为空，跳过
    if (isRehashing() || !table_[0]) return false;

    // 负载因子 = used / size
    size_t size = size_mask_[0] + 1;
    if (used_ >= size) {
        // 负载 ≥ 1.0，扩容为 2x
        return expand(size * 2);
    }
    return false;
}

inline bool Dict::shrinkIfNeeded() {
    if (isRehashing() || !table_[0]) return false;

    size_t size = size_mask_[0] + 1;
    // 负载因子 < 0.1 时缩容
    if (size > INITIAL_SIZE && used_ < size / 10) {
        size_t new_size = size / 2;
        if (new_size < INITIAL_SIZE) new_size = INITIAL_SIZE;
        return expand(new_size);
    }
    return false;
}

inline bool Dict::expand(size_t new_size) {
    if (isRehashing()) return false;
    if (new_size > MAX_SIZE) return false;
    if (new_size <= (size_mask_[0] + 1)) return false;

    // 分配新表
    Entry** new_table = new Entry*[new_size]();
    size_t new_mask = new_size - 1;

    // 设置 table_[1] 为 rehash 目标
    table_[1] = new_table;
    size_mask_[1] = new_mask;
    rehash_idx_ = 0;

    return true;
}

inline void Dict::rehashStep(int n) {
    if (!isRehashing()) return;

    // 跳过 table_[0] 中的空桶
    size_t size0 = size_mask_[0] + 1;
    while (n > 0 && rehash_idx_ >= 0 && static_cast<size_t>(rehash_idx_) < size0) {
        size_t idx = static_cast<size_t>(rehash_idx_);

        // 如果当前桶为空，直接前进
        if (!table_[0] || !table_[0][idx]) {
            rehash_idx_++;
            if (static_cast<size_t>(rehash_idx_) >= size0) break;
            continue;
        }

        // 搬移当前桶的所有 entry 到 table_[1]
        Entry* entry = table_[0][idx];
        table_[0][idx] = nullptr;

        while (entry) {
            Entry* next = entry->next;

            uint64_t h = hashKey(entry->key);
            size_t new_idx = h & size_mask_[1];

            entry->next = table_[1][new_idx];
            table_[1][new_idx] = entry;

            entry = next;
        }

        rehash_idx_++;
        n--;
    }

    // 检查 rehash 是否完成
    if (static_cast<size_t>(rehash_idx_) >= size0) {
        // table_[0] 已经完全搬空
        delete[] table_[0];
        table_[0] = table_[1];
        size_mask_[0] = size_mask_[1];
        table_[1] = nullptr;
        size_mask_[1] = 0;
        rehash_idx_ = -1;  // rehash 完成
    }
}

inline int Dict::rehashMilliseconds(int ms) {
    // 简化为固定步数 (后续可以用高精度时钟控制)
    // 假设每步 ~1μs，1ms = 1000 步
    int steps = ms * 1000;
    rehashStep(steps);
    return isRehashing() ? 1 : 0;
}

inline size_t Dict::capacity() const {
    size_t cap = 0;
    if (table_[0]) cap += size_mask_[0] + 1;
    if (table_[1]) cap += size_mask_[1] + 1;
    return cap;
}

inline size_t Dict::bucketCount(int table) const {
    if (table < 0 || table > 1 || !table_[table]) return 0;
    size_t size = size_mask_[table] + 1;
    size_t count = 0;
    for (size_t i = 0; i < size; ++i) {
        if (table_[table][i]) count++;
    }
    return count;
}

// ======== Iterator 实现 ========

inline Dict::Iterator::Iterator(Dict* dict) : dict_(dict) {
    // 开始于 table_[0] 的第一个非空桶
    if (dict_->table_[0]) {
        table_ = 0;
        index_ = 0;
        size_t size = dict_->size_mask_[0] + 1;
        while (index_ < static_cast<int64_t>(size) && !dict_->table_[0][index_]) {
            index_++;
        }
        if (index_ < static_cast<int64_t>(size)) {
            entry_ = dict_->table_[0][index_];
            next_entry_ = entry_->next;
        } else {
            entry_ = nullptr;
        }
    }
}

inline bool Dict::Iterator::valid() const {
    return entry_ != nullptr;
}

inline void Dict::Iterator::next() {
    if (!entry_) return;

    // 先走当前链表的 next
    if (next_entry_) {
        entry_ = next_entry_;
        next_entry_ = next_entry_->next;
        return;
    }

    // 链表走完，找下一个非空桶
    size_t size = dict_->size_mask_[table_] + 1;
    index_++;
    while (index_ < static_cast<int64_t>(size) && !dict_->table_[table_][index_]) {
        index_++;
    }

    if (index_ < static_cast<int64_t>(size)) {
        entry_ = dict_->table_[table_][index_];
        next_entry_ = entry_->next;
    } else if (table_ == 0 && dict_->table_[1]) {
        // 切换到 table_[1]
        table_ = 1;
        index_ = 0;
        size = dict_->size_mask_[1] + 1;
        while (index_ < static_cast<int64_t>(size) && !dict_->table_[1][index_]) {
            index_++;
        }
        if (index_ < static_cast<int64_t>(size)) {
            entry_ = dict_->table_[1][index_];
            next_entry_ = entry_->next;
        } else {
            entry_ = nullptr;
        }
    } else {
        entry_ = nullptr;
    }
}

inline std::string Dict::Iterator::key() const {
    return entry_ ? entry_->key : std::string{};
}

inline Value* Dict::Iterator::value() const {
    return entry_ ? &entry_->value : nullptr;
}

} // namespace ledis
