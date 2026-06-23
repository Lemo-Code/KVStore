#include <mutex>
// memory_engine.h — In-memory storage engine with ordered map + hash map
#pragma once

#include <map>
#include <unordered_map>
#include <shared_mutex>
#include <string>
#include <vector>

#include "kvstore/common/kv_types.h"
#include "kvstore/common/kv_error.h"
#include "kvstore/storage/storage_engine.h"

namespace zero {
namespace kvstore {

// ============================================================
// MemoryEngine — thread-safe, purely in-memory KV store
//
// Maintains two indexes:
//   1. std::map<Key, Value>  — ordered, supports range scans
//   2. std::unordered_map<Key, Value*> — O(1) point lookups
//       (values point into the ordered map)
//
// Thread safety: std::shared_mutex with reader/writer locking.
// ============================================================
class MemoryEngine : public IKvEngine {
public:
    MemoryEngine() = default;
    ~MemoryEngine() override = default;

    // ---- single-key operations ----
    Status Put(const Key& key, const Value& val) override;
    Status Get(const Key& key, Value& val) override;
    Status Delete(const Key& key) override;

    // ---- batch (atomic) ----
    Status ApplyBatch(const Batch& batch) override;

    // ---- scan ----
    // start: inclusive   limit: exclusive (empty = unbounded)
    Status Scan(const Key& start, const Key& limit, size_t max_count,
                std::vector<KeyValue>& results) override;

    // ---- snapshot / restore ----
    // Format: [varint64 count][for each: varint64 key_len, key,
    //          varint64 val_len, val]
    Status Snapshot(std::string& data) override;
    Status RestoreSnapshot(const std::string& data) override;

    // ---- stats ----
    size_t KeyCount()    const override;
    size_t MemoryUsage() const override;

    // ---- lifecycle ----
    Status Close() override;

private:
    // Ordered storage for range scans
    std::map<Key, Value> map_;

    // Hash index for O(1) lookups — points into map_
    std::unordered_map<Key, Value*> hash_index_;

    // Reader-writer lock
    mutable std::shared_mutex mutex_;

    // For memory usage tracking (approximate)
    size_t total_value_bytes_ = 0;

    // Internal helpers — caller must hold at least a shared_lock
    void RebuildHashIndex();
    void RecordPut(const Key& key, const Value& val);
    void RecordDelete(const Key& key);
};

} // namespace kvstore
} // namespace zero
