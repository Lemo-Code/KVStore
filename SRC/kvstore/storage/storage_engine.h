// storage_engine.h — Abstract storage engine interface
#pragma once

#include "kvstore/common/kv_types.h"
#include "kvstore/common/kv_error.h"

namespace zero {
namespace kvstore {

// ============================================================
// IKvEngine — pluggable storage backend
// ============================================================
class IKvEngine {
public:
    virtual ~IKvEngine() = default;

    // ---- single-key operations ----
    virtual Status Put(const Key& key, const Value& val) = 0;
    virtual Status Get(const Key& key, Value& val) = 0;
    virtual Status Delete(const Key& key) = 0;

    // ---- batch (atomic) ----
    virtual Status ApplyBatch(const Batch& batch) = 0;

    // ---- scan ----
    virtual Status Scan(const Key& start, const Key& limit, size_t max_count,
                        std::vector<KeyValue>& results) = 0;

    // ---- snapshot / restore ----
    virtual Status Snapshot(std::string& data) = 0;
    virtual Status RestoreSnapshot(const std::string& data) = 0;

    // ---- stats ----
    virtual size_t KeyCount() const = 0;
    virtual size_t MemoryUsage() const = 0;

    // ---- lifecycle ----
    virtual Status Close() = 0;
};

} // namespace kvstore
} // namespace zero
