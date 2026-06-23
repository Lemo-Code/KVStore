#pragma once
// ============================================================
// cluster_types.h — 分布式集群共享类型定义
// ============================================================

#include <cstdint>
#include <string>
#include <lstl/container/vector.h>
#include <lstl/container/unordered_map.h>

namespace ledis {
namespace cluster {

static constexpr uint16_t HASH_SLOTS      = 16384;
static constexpr uint16_t SLOT_BITMAP_WORDS = 256;  // 16384 / 64

// ---- 节点状态 ----
enum class NodeState : uint8_t {
    UNKNOWN = 0,
    JOINING = 1,
    ACTIVE  = 2,
    PFAIL   = 3,  // 主观下线
    FAIL    = 4,  // 客观下线 (多数确认)
};

inline const char* nodeStateName(NodeState s) {
    switch (s) {
    case NodeState::UNKNOWN: return "unknown";
    case NodeState::JOINING: return "joining";
    case NodeState::ACTIVE:  return "active";
    case NodeState::PFAIL:   return "pfail";
    case NodeState::FAIL:    return "fail";
    }
    return "?";
}

// ---- 节点信息 ----
struct NodeInfo {
    std::string id;
    std::string ip;
    int         client_port  = 0;
    int         cluster_port = 0;
    NodeState   state        = NodeState::UNKNOWN;
    uint64_t    epoch        = 0;
    bool        is_master    = true;
    std::string master_id;   // 副本节点记录其主节点 id

    // 心跳时间戳
    uint64_t ping_sent_ms     = 0;
    uint64_t pong_received_ms = 0;

    // 槽位位图: 256 个 uint64_t = 16384 bits
    uint64_t slots[SLOT_BITMAP_WORDS]{};

    bool ownsSlot(uint16_t slot) const {
        if (slot >= HASH_SLOTS) return false;
        return (slots[slot >> 6] >> (slot & 63)) & 1ULL;
    }

    void setSlot(uint16_t slot, bool owned) {
        if (slot >= HASH_SLOTS) return;
        if (owned)
            slots[slot >> 6] |=  (1ULL << (slot & 63));
        else
            slots[slot >> 6] &= ~(1ULL << (slot & 63));
    }

    void clearSlots() {
        for (int i = 0; i < SLOT_BITMAP_WORDS; ++i) slots[i] = 0;
    }

    int slotCount() const {
        int cnt = 0;
        for (int i = 0; i < SLOT_BITMAP_WORDS; ++i)
            cnt += __builtin_popcountll(slots[i]);
        return cnt;
    }

    void getSlotList(lstl::vector<uint16_t>& out) const {
        for (uint16_t s = 0; s < HASH_SLOTS; ++s)
            if (ownsSlot(s)) out.push_back(s);
    }
};

} // namespace cluster
} // namespace ledis
