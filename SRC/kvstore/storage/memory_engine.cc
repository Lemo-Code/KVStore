#include <mutex>
#include <shared_mutex>
// memory_engine.cc — In-memory storage engine implementation
#include "kvstore/storage/memory_engine.h"
#include "kvstore/common/kv_utils.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace zero {
namespace kvstore {

// ---- internal helpers ----

void MemoryEngine::RebuildHashIndex() {
    hash_index_.clear();
    // Reserve space to avoid rehashing
    hash_index_.reserve(map_.size());
    for (auto& kv : map_) {
        hash_index_.emplace(kv.first, &kv.second);
    }
}

void MemoryEngine::RecordPut(const Key& key, const Value& val) {
    // Erase old hash entry and value tracking if key existed
    auto it = hash_index_.find(key);
    if (it != hash_index_.end()) {
        total_value_bytes_ -= it->second->size();
        map_.erase(key);
    }
    auto [map_it, inserted] = map_.emplace(key, val);
    if (!inserted) {
        // Key already existed; update value
        map_it->second = val;
    }
    hash_index_[key] = &map_it->second;
    total_value_bytes_ += val.size();
}

void MemoryEngine::RecordDelete(const Key& key) {
    auto hash_it = hash_index_.find(key);
    if (hash_it != hash_index_.end()) {
        total_value_bytes_ -= hash_it->second->size();
        map_.erase(key);
        hash_index_.erase(hash_it);
    }
}

// ============================================================
// Single-key operations
// ============================================================

Status MemoryEngine::Put(const Key& key, const Value& val) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    RecordPut(key, val);
    return Status::OK();
}

Status MemoryEngine::Get(const Key& key, Value& val) {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = hash_index_.find(key);
    if (it == hash_index_.end()) {
        return Status::NotFound(key);
    }
    val = *it->second;
    return Status::OK();
}

Status MemoryEngine::Delete(const Key& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (hash_index_.find(key) == hash_index_.end()) {
        return Status::NotFound(key);
    }
    RecordDelete(key);
    return Status::OK();
}

// ============================================================
// Batch operations (atomic)
// ============================================================

Status MemoryEngine::ApplyBatch(const Batch& batch) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    for (const auto& entry : batch) {
        switch (entry.op) {
            case BatchOp::PUT: {
                // Insert or update
                auto map_it = map_.find(entry.key);
                if (map_it != map_.end()) {
                    total_value_bytes_ -= map_it->second.size();
                    map_it->second = entry.val;
                } else {
                    auto [new_it, _] = map_.emplace(entry.key, entry.val);
                    map_it = new_it;
                }
                hash_index_[entry.key] = &map_it->second;
                total_value_bytes_  += entry.val.size();
                break;
            }
            case BatchOp::DEL: {
                // Delete if exists
                auto hash_it = hash_index_.find(entry.key);
                if (hash_it != hash_index_.end()) {
                    total_value_bytes_ -= hash_it->second->size();
                    map_.erase(entry.key);
                    hash_index_.erase(hash_it);
                }
                break;
            }
        }
    }

    return Status::OK();
}

// ============================================================
// Scan
// ============================================================

Status MemoryEngine::Scan(const Key& start, const Key& limit,
                           size_t max_count,
                           std::vector<KeyValue>& results) {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    results.clear();
    if (max_count == 0) {
        return Status::OK();
    }

    // Find starting position via lower_bound
    auto it = map_.lower_bound(start);

    for (size_t i = 0; i < max_count && it != map_.end(); ++i, ++it) {
        // Check limit bound (exclusive)
        if (!limit.empty() && it->first >= limit) {
            break;
        }
        results.emplace_back(it->first, it->second);
    }

    return Status::OK();
}

// ============================================================
// Snapshot / Restore
// ============================================================

Status MemoryEngine::Snapshot(std::string& data) {
    std::shared_lock<std::shared_mutex> lock(mutex_);

    data.clear();

    // Reserve approximate space to minimise reallocations
    data.reserve(map_.size() * 64);

    // Write number of entries as varint64
    EncodeVarint64(static_cast<uint64_t>(map_.size()), data);

    // Write each key-value pair: key_len, key, val_len, val
    for (const auto& kv : map_) {
        EncodeVarint64(kv.first.size(), data);
        data.append(kv.first);
        EncodeVarint64(kv.second.size(), data);
        data.append(kv.second);
    }

    return Status::OK();
}

Status MemoryEngine::RestoreSnapshot(const std::string& data) {
    std::unique_lock<std::shared_mutex> lock(mutex_);

    // Clear existing data
    map_.clear();
    hash_index_.clear();
    total_value_bytes_ = 0;

    if (data.empty()) {
        return Status::OK();
    }

    const char* p = data.data();
    const char* end = data.data() + data.size();

    // Read entry count
    uint64_t count = 0;
    if (p < end) {
        count = DecodeVarint64(p, end);
    }

    // map_ has no reserve, preallocate via hint: (void)(static_cast<size_t>(count));
    hash_index_.reserve(static_cast<size_t>(count));

    for (uint64_t i = 0; i < count && p < end; ++i) {
        // Read key
        uint64_t key_len = DecodeVarint64(p, end);
        if (p + key_len > end) {
            return Status::IOError("snapshot: truncated key");
        }
        Key key(p, static_cast<size_t>(key_len));
        p += key_len;

        // Read value
        uint64_t val_len = DecodeVarint64(p, end);
        if (p + val_len > end) {
            return Status::IOError("snapshot: truncated value");
        }
        Value val(p, static_cast<size_t>(val_len));
        p += val_len;

        // Insert into map
        auto [map_it, _] = map_.emplace(std::move(key), std::move(val));
        hash_index_.emplace(map_it->first, &map_it->second);
        total_value_bytes_ += map_it->second.size();
    }

    return Status::OK();
}

// ============================================================
// Stats
// ============================================================

size_t MemoryEngine::KeyCount() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return map_.size();
}

size_t MemoryEngine::MemoryUsage() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    // Estimate: map nodes + hash table overhead + value bytes
    // std::map node ≈ 3 pointers + key + value ≈ 24 + key + value
    // std::unordered_map entry ≈ pointer + key ≈ 8 + key
    size_t usage = total_value_bytes_;

    // Keys in map_ also consume space; sum them separately
    // We approximate by key sizes (already in both containers)
    for (const auto& kv : map_) {
        usage += kv.first.capacity();         // key storage
        usage += kv.second.capacity();        // value storage
        usage += 40;  // map node overhead (RB-tree node ≈ 3 ptrs + color)
    }
    usage += hash_index_.size() * 32;  // hash bucket/entry overhead

    return usage;
}

Status MemoryEngine::Close() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    map_.clear();
    hash_index_.clear();
    total_value_bytes_ = 0;
    return Status::OK();
}

} // namespace kvstore
} // namespace zero
