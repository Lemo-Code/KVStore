#pragma once

#include <cstdint>
#include <string>
#include <lstl/container/unordered_map.h>
#include <lstl/container/unordered_set.h>
#include <lstl/container/deque.h>
#include <lstl/container/set.h>

namespace ledis {

// ============================================================
// ValueType
// ============================================================
enum class ValueType : uint8_t {
    STRING = 0, LIST = 1, HASH = 2, SET = 3, ZSET = 4,
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
    lstl::deque<std::string> elements;
};

struct SetData {
    lstl::unordered_set<std::string> members;
};

struct ZSetData {
    // 有序集合: (score, member) 按 score→member 字典序排列
    lstl::set<std::pair<double, std::string>> by_score;
    // member → score 映射 (O(1) 查找)
    lstl::unordered_map<std::string, double> scores;
};

// ============================================================
// Value — 统一值类型 (move-only, 多态存储)
// ============================================================
struct Value {
    ValueType type = ValueType::STRING;
    std::string str;
    int64_t     int_val = 0;
    void*       opaque_ptr = nullptr;

    // 元数据
    uint64_t expire_at_ms = 0;
    uint32_t lru = 0;
    uint8_t  lfu = 0;

    // ---- 工厂方法 ----
    static Value createString(std::string s) {
        Value v; v.type = ValueType::STRING; v.str = std::move(s); return v;
    }
    static Value createInt(int64_t n) {
        Value v; v.type = ValueType::STRING;
        v.int_val = n; v.str = std::to_string(n); return v;
    }
    static Value createHash() {
        Value v; v.type = ValueType::HASH; v.opaque_ptr = new HashData(); return v;
    }
    static Value createList() {
        Value v; v.type = ValueType::LIST; v.opaque_ptr = new ListData(); return v;
    }
    static Value createSet() {
        Value v; v.type = ValueType::SET; v.opaque_ptr = new SetData(); return v;
    }
    static Value createZSet() {
        Value v; v.type = ValueType::ZSET; v.opaque_ptr = new ZSetData(); return v;
    }

    // ---- 类型安全访问器 ----
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
        case ValueType::HASH: delete static_cast<HashData*>(opaque_ptr); break;
        case ValueType::LIST: delete static_cast<ListData*>(opaque_ptr); break;
        case ValueType::SET:  delete static_cast<SetData*>(opaque_ptr);  break;
        case ValueType::ZSET: delete static_cast<ZSetData*>(opaque_ptr); break;
        default: break;
        }
        opaque_ptr = nullptr;
    }

    ~Value() { destroy(); }
    Value() = default;

    Value(Value&& other) noexcept
        : type(other.type), str(std::move(other.str)),
          int_val(other.int_val), opaque_ptr(other.opaque_ptr),
          expire_at_ms(other.expire_at_ms), lru(other.lru), lfu(other.lfu) {
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
            lru = other.lru;
            lfu = other.lfu;
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
