#pragma once
// ============================================================
// cluster_topology.h — 集群拓扑管理 (槽位分布 + 成员状态)
// ============================================================
//
// 线程安全: 所有读写都在 server 线程中进行。
// RPC I/O 线程不直接访问拓扑 — 通过消息队列间接通信。
//

#include <cstdint>
#include <string>
#include <string_view>
#include <mutex>
#include <lstl/container/vector.h>
#include <lstl/container/unordered_map.h>

#include "ledis/core/storage_engine.h"
#include "ledis/cluster/cluster_types.h"

namespace ledis {
namespace cluster {

class ClusterTopology {
public:
    ClusterTopology() {
        for (uint16_t i = 0; i < HASH_SLOTS; ++i)
            slot_owner_[i].clear();
    }

    // 锁 (保护 nodes_ 和 slot_owner_，I/O 线程和 server 线程共享)
    std::mutex mtx;

    // ---- 槽位查询 ----

    static uint16_t keySlot(std::string_view key) {
        return StorageEngine::getSlot(key);
    }

    // 以下方法不用 const: lstl const_iterator 有类型转换 bug
    const std::string* slotOwner(uint16_t slot) {
        if (slot >= HASH_SLOTS) return nullptr;
        if (!slot_owner_[slot].empty()) return &slot_owner_[slot];
        // 自愈: bitmap 有但 slot_owner_ 未同步
        for (auto& kv : nodes_) {
            if (kv.second.is_master && kv.second.ownsSlot(slot)) {
                slot_owner_[slot] = kv.second.id;
                return &slot_owner_[slot];
            }
        }
        return nullptr;
    }

    const std::string* keyOwner(std::string_view key) {
        return slotOwner(keySlot(key));
    }

    bool isLocalSlot(uint16_t slot) {
        if (slot >= HASH_SLOTS) return false;
        const auto* n = slotOwner(slot);
        if (n) return *n == self_id_;
        for (auto& kv : nodes_) {
            if (kv.first == self_id_ && kv.second.ownsSlot(slot))
                return true;
        }
        return false;
    }

    bool isLocalKey(std::string_view key) {
        return isLocalSlot(keySlot(key));
    }

    // ---- 槽位分配 ----

    void setSlot(uint16_t slot, const std::string& node_id, bool assign) {
        if (slot >= HASH_SLOTS) return;

        // 清除旧所有者位图
        const std::string& old = slot_owner_[slot];
        if (!old.empty()) {
            auto it = nodes_.find(old);
            if (it != nodes_.end())
                it->second.setSlot(slot, false);
        }

        if (assign && !node_id.empty()) {
            slot_owner_[slot] = node_id;
            auto it = nodes_.find(node_id);
            if (it != nodes_.end())
                it->second.setSlot(slot, true);
        } else {
            slot_owner_[slot].clear();
        }
    }

    void assignSlots(const std::string& node_id, const lstl::vector<uint16_t>& slots) {
        for (auto s : slots) setSlot(s, node_id, true);
    }

    // 分配全部槽位给本节点 (首个节点引导)
    void assignAllSlotsToSelf() {
        for (uint16_t s = 0; s < HASH_SLOTS; ++s)
            setSlot(s, self_id_, true);
    }

    bool isFullCoverage() const {
        for (uint16_t s = 0; s < HASH_SLOTS; ++s)
            if (slot_owner_[s].empty()) return false;
        return true;
    }

    // ---- 节点管理 ----

    void addNode(const NodeInfo& node) {
        // 保护 self node: 关键状态 (is_master/master_id/slots/client_port)
        // 由本地操作修改, 不被 gossip 数据覆盖
        if (node.id == self_id_) {
            auto it = nodes_.find(node.id);
            if (it != nodes_.end()) {
                // 保存本地状态
                bool was_master = it->second.is_master;
                std::string was_master_id = it->second.master_id;
                int was_client_port = it->second.client_port;
                uint64_t was_slots[SLOT_BITMAP_WORDS];
                memcpy(was_slots, it->second.slots, sizeof(was_slots));

                // 更新远程字段
                it->second.state = node.state;
                it->second.ping_sent_ms = node.ping_sent_ms;
                it->second.pong_received_ms = node.pong_received_ms;
                if (node.epoch > it->second.epoch)
                    it->second.epoch = node.epoch;

                // 恢复本地状态
                it->second.is_master = was_master;
                it->second.master_id = was_master_id;
                it->second.client_port = (was_client_port > 0) ? was_client_port : node.client_port;
                memcpy(it->second.slots, was_slots, sizeof(was_slots));
                return;
            }
        }

        auto it = nodes_.find(node.id);
        if (it != nodes_.end()) {
            if (it->second.epoch > node.epoch) return;
            // 保留 self node 的关键状态
            bool was_master = it->second.is_master;
            std::string was_master_id = it->second.master_id;
            uint64_t was_slots[SLOT_BITMAP_WORDS];
            memcpy(was_slots, it->second.slots, sizeof(was_slots));
            int was_client_port = it->second.client_port;

            nodes_[node.id] = node;
            if (node.id == self_id_) {
                nodes_[node.id].is_master = was_master;
                nodes_[node.id].master_id = was_master_id;
                nodes_[node.id].client_port = (was_client_port > 0) ? was_client_port : node.client_port;
                memcpy(nodes_[node.id].slots, was_slots, sizeof(was_slots));
            }
        } else {
            nodes_[node.id] = node;
        }

        // 更新槽位: 仅同步到 slot_owner_ (不清除已有归属)
        if (nodes_[node.id].is_master) {
            for (uint16_t s = 0; s < HASH_SLOTS; ++s) {
                if (nodes_[node.id].ownsSlot(s)) {
                    if (slot_owner_[s].empty())
                        slot_owner_[s] = node.id;
                }
            }
        }
    }

    void removeNode(const std::string& node_id) {
        for (uint16_t s = 0; s < HASH_SLOTS; ++s) {
            if (slot_owner_[s] == node_id)
                slot_owner_[s].clear();
        }
        nodes_.erase(node_id);
    }

    void updateState(const std::string& node_id, NodeState state) {
        auto it = nodes_.find(node_id);
        if (it != nodes_.end()) it->second.state = state;
    }

    // 注意: lstl const_iterator 有转换问题，以下方法不使用 const
    const NodeInfo* getNode(const std::string& node_id) {
        for (auto& kv : nodes_)
            if (kv.first == node_id) return &kv.second;
        return nullptr;
    }

    NodeInfo* getNodeMutable(const std::string& node_id) {
        auto it = nodes_.find(node_id);
        return (it != nodes_.end()) ? &it->second : nullptr;
    }

    lstl::unordered_map<std::string, NodeInfo>& allNodes() { return nodes_; }

    // ---- 本节点身份 ----

    void setSelfId(const std::string& id)   { self_id_ = id; }
    const std::string& selfId() const       { return self_id_; }
    const NodeInfo*     selfNode()          { return getNode(self_id_); }
    NodeInfo*           selfNodeMut()       { return getNodeMutable(self_id_); }

    // ---- Epoch ----

    uint64_t epoch() const { return epoch_; }
    void bumpEpoch()       { epoch_++; }
    void setEpoch(uint64_t e) { if (e > epoch_) epoch_ = e; }

    // ---- 序列化 (用于 nodes.conf 持久化) ----
    std::string serializeNode(const NodeInfo& node) const;
    bool deserializeNode(const std::string& data, NodeInfo& node) const;

private:
    std::string self_id_;
    lstl::unordered_map<std::string, NodeInfo> nodes_;
    std::string slot_owner_[HASH_SLOTS];
    uint64_t epoch_ = 0;
};

} // namespace cluster
} // namespace ledis
