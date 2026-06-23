// kv_config.cc — YAML config loader implementation
#include "kvstore/config/kv_config.h"

#include <fstream>
#include <sstream>
#include <iostream>

#include "yaml-cpp/yaml.h"

namespace zero {
namespace kvstore {

// ---- helpers ----
namespace {

// Try to read a scalar value from a YAML node, returning the default if missing
// or of wrong type.
template <typename T>
T YmlOpt(const YAML::Node& node, const std::string& key, T fallback) {
    if (!node || !node[key]) return fallback;
    try {
        return node[key].as<T>();
    } catch (const YAML::Exception&) {
        // Type mismatch or parse error — silently use fallback
        return fallback;
    }
}

// Specialisation for std::string to avoid ambiguity with const char*
inline std::string YmlOptStr(const YAML::Node& node, const std::string& key,
                             const std::string& fallback) {
    if (!node || !node[key]) return fallback;
    try {
        return node[key].as<std::string>();
    } catch (const YAML::Exception&) {
        return fallback;
    }
}

// Read a sequence of strings from a YAML key
std::vector<std::string> YmlOptStrSeq(const YAML::Node& node,
                                      const std::string& key) {
    std::vector<std::string> result;
    if (!node || !node[key]) return result;
    try {
        for (const auto& item : node[key]) {
            result.push_back(item.as<std::string>());
        }
    } catch (const YAML::Exception&) {
        // Return whatever we parsed so far (or empty)
    }
    return result;
}

} // anonymous namespace

Status LoadConfig(const std::string& path, KvConfig& cfg) {
    // If the file does not exist, return with defaults already in cfg
    std::ifstream fin(path);
    if (!fin.good()) {
        // File not found is not an error — use defaults
        return Status::OK();
    }

    YAML::Node doc;
    try {
        doc = YAML::LoadFile(path);
    } catch (const YAML::Exception& e) {
        return Status::IOError(std::string("YAML parse error: ") + e.what());
    }

    if (!doc.IsMap()) {
        // Empty or non-map document — still not an error, keep defaults
        return Status::OK();
    }

    // ---- node identity ----
    cfg.node_id = YmlOptStr(doc, "node_id", cfg.node_id);

    // ---- networking ----
    cfg.listen_host = YmlOptStr(doc, "listen_host", cfg.listen_host);
    {
        int64_t port = YmlOpt<int64_t>(doc, "listen_port", -1);
        if (port > 0 && port <= 65535) cfg.listen_port = static_cast<uint16_t>(port);
    }
    {
        int64_t port = YmlOpt<int64_t>(doc, "raft_port", -1);
        if (port > 0 && port <= 65535) cfg.raft_port = static_cast<uint16_t>(port);
    }

    // ---- peers ----
    {
        auto peers = YmlOptStrSeq(doc, "peer_addrs");
        if (!peers.empty()) cfg.peer_addrs = std::move(peers);
    }

    // ---- Raft timing ----
    cfg.raft_heartbeat_ms = YmlOpt<int64_t>(doc, "raft_heartbeat_ms", cfg.raft_heartbeat_ms);
    cfg.raft_election_min_ms = YmlOpt<int64_t>(doc, "raft_election_min_ms", cfg.raft_election_min_ms);
    cfg.raft_election_max_ms = YmlOpt<int64_t>(doc, "raft_election_max_ms", cfg.raft_election_max_ms);
    cfg.raft_rpc_timeout_ms = YmlOpt<int64_t>(doc, "raft_rpc_timeout_ms", cfg.raft_rpc_timeout_ms);

    // ---- storage paths ----
    cfg.wal_dir  = YmlOptStr(doc, "wal_dir", cfg.wal_dir);
    cfg.data_dir = YmlOptStr(doc, "data_dir", cfg.data_dir);

    // ---- snapshot / log limits ----
    cfg.snapshot_entries = YmlOpt<size_t>(doc, "snapshot_entries", cfg.snapshot_entries);
    cfg.max_log_entries  = YmlOpt<size_t>(doc, "max_log_entries",  cfg.max_log_entries);

    // ---- sharding ----
    cfg.shard_count   = YmlOpt<int>(doc, "shard_count",   cfg.shard_count);
    cfg.replica_count = YmlOpt<int>(doc, "replica_count", cfg.replica_count);

    // ---- logging ----
    cfg.log_level = YmlOptStr(doc, "log_level", cfg.log_level);

    return cfg.Validate();
}

} // namespace kvstore
} // namespace zero
