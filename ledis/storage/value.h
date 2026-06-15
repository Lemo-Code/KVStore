#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>

#include <lstl/container/unordered_map.h>
#include <lstl/container/unordered_set.h>
#include <lstl/container/deque.h>
#include <lstl/container/list.h>
#include <lstl/container/set.h>

namespace ledis {

// ============================================================
// ValueType
// ============================================================
enum class ValueType : uint8_t {
    STRING = 0,
    LIST   = 1,
    HASH   = 2,
    SET    = 3,
    ZSET   = 4,
};

inline const char* valueTypeName(ValueType t) {
    switch (t) {
        case ValueType::STRING: return "string";
        case ValueType::LIST:   return "list";
        case ValueType::HASH:   return "hash";
        case ValueType::SET:    return "set";
        case ValueType::ZSET:   return "zset";
        default:                return "none";
    }
}

// ============================================================
// 类型特定的内部数据结构
// ============================================================
struct HashData {
    lstl::unordered_map<std::string, std::string> fields;
};

struct ListData {
    lstl::list<std::string> elements;
};

struct SetData {
    lstl::unordered_set<std::string> members;
};

struct ZSetEntry {
    std::string member;
    double      score;
    bool operator<(const ZSetEntry& o) const {
        if (score != o.score) return score < o.score;
        return member < o.member;
    }
};

struct ZSetData {
    lstl::set<ZSetEntry>                        by_score;  // 按 score 排序
    lstl::unordered_map<std::string, double>     scores;   // member → score (O(1))
};

// ============================================================
// Value — 统一值类型 (move-only)
// ============================================================
struct Value {
    ValueType type = ValueType::STRING;

    // 字符串/内联数据
    std::string str;
    int64_t     int_val = 0;

    // 复杂类型指针 (Hash/List/Set/ZSet)
    void* opaque_ptr = nullptr;

    // 元数据
    uint64_t expire_at_ms = 0;
    uint32_t lru = 0;       // LRU 时钟 (24bit 实际使用, 存储线程更新)
    uint8_t  lfu = 0;       // LFU 频率计数

    // ---- 工厂方法 ----

    static Value createString(std::string s) {
        Value v;
        v.type = ValueType::STRING;
        v.str = std::move(s);
        return v;
    }

    static Value createInt(int64_t n) {
        Value v;
        v.type = ValueType::STRING;
        v.int_val = n;
        v.str = std::to_string(n);
        return v;
    }

    static Value createHash() {
        Value v;
        v.type = ValueType::HASH;
        v.opaque_ptr = new HashData();
        return v;
    }

    static Value createList() {
        Value v;
        v.type = ValueType::LIST;
        v.opaque_ptr = new ListData();
        return v;
    }

    static Value createSet() {
        Value v;
        v.type = ValueType::SET;
        v.opaque_ptr = new SetData();
        return v;
    }

    static Value createZSet() {
        Value v;
        v.type = ValueType::ZSET;
        v.opaque_ptr = new ZSetData();
        return v;
    }

    // ---- 类型转换辅助 ----

    HashData* asHash() const {
        return (type == ValueType::HASH) ? static_cast<HashData*>(opaque_ptr) : nullptr;
    }
    ListData* asList() const {
        return (type == ValueType::LIST) ? static_cast<ListData*>(opaque_ptr) : nullptr;
    }
    SetData* asSet() const {
        return (type == ValueType::SET) ? static_cast<SetData*>(opaque_ptr) : nullptr;
    }
    ZSetData* asZSet() const {
        return (type == ValueType::ZSET) ? static_cast<ZSetData*>(opaque_ptr) : nullptr;
    }

    // ---- 生命周期 ----

    void destroy() {
        switch (type) {
            case ValueType::HASH:  delete static_cast<HashData*>(opaque_ptr); break;
            case ValueType::LIST:  delete static_cast<ListData*>(opaque_ptr); break;
            case ValueType::SET:   delete static_cast<SetData*>(opaque_ptr);  break;
            case ValueType::ZSET:  delete static_cast<ZSetData*>(opaque_ptr); break;
            default: break;
        }
        opaque_ptr = nullptr;
    }

    ~Value() { destroy(); }

    Value() = default;

    Value(Value&& other) noexcept
        : type(other.type)
        , str(std::move(other.str))
        , int_val(other.int_val)
        , opaque_ptr(other.opaque_ptr)
        , expire_at_ms(other.expire_at_ms)
    {
        other.opaque_ptr = nullptr;
        other.type = ValueType::STRING;
    }

    Value& operator=(Value&& other) noexcept {
        if (this != &other) {
            destroy();
            type = other.type;
            str = std::move(other.str);
            int_val = other.int_val;
            opaque_ptr = other.opaque_ptr;
            expire_at_ms = other.expire_at_ms;
            other.opaque_ptr = nullptr;
            other.type = ValueType::STRING;
        }
        return *this;
    }

    Value(const Value&) = delete;
    Value& operator=(const Value&) = delete;

    // ---- 过期 ----

    bool isExpired(uint64_t now_ms) const {
        return expire_at_ms > 0 && now_ms >= expire_at_ms;
    }
    int64_t ttlSec(uint64_t now_ms) const {
        if (expire_at_ms == 0) return -1;
        if (now_ms >= expire_at_ms) return -2;
        return static_cast<int64_t>((expire_at_ms - now_ms) / 1000);
    }
    int64_t ttlMs(uint64_t now_ms) const {
        if (expire_at_ms == 0) return -1;
        if (now_ms >= expire_at_ms) return -2;
        return static_cast<int64_t>(expire_at_ms - now_ms);
    }
};

} // namespace ledis
