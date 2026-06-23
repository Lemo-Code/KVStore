// kv_config.h — Configuration struct and loader for the distributed KV store
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <random>

#include "kvstore/common/kv_types.h"
#include "kvstore/common/kv_error.h"

namespace zero {
namespace kvstore {

// ============================================================
// KvConfig — all tunables for a KV node
// ============================================================
struct KvConfig {
    // ---- node identity ----
    NodeId node_id;

    // ---- networking ----
    std::string listen_host = "0.0.0.0";
    uint16_t    listen_port = 9700;
    uint16_t    raft_port   = 9701;

    // ---- peer addresses (host:port strings) ----
    std::vector<std::string> peer_addrs;

    // ---- Raft timing ----
    int64_t raft_heartbeat_ms     = 100;
    int64_t raft_election_min_ms  = 500;
    int64_t raft_election_max_ms  = 1000;
    int64_t raft_rpc_timeout_ms   = 300;

    // ---- storage paths ----
    std::string wal_dir  = "./data/wal";
    std::string data_dir = "./data";

    // ---- snapshot / log limits ----
    size_t snapshot_entries = 10000;
    size_t max_log_entries  = 100000;

    // ---- sharding ----
    int shard_count   = 8;
    int replica_count = 3;

    // ---- logging ----
    std::string log_level = "info";

    // ---- helpers ----
    int64_t RandomizedElectionTimeout() const {
        static thread_local std::mt19937_64 rng(
            static_cast<uint64_t>(NowMs()) ^
            static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this)));
        std::uniform_int_distribution<int64_t> dist(raft_election_min_ms,
                                                     raft_election_max_ms);
        return dist(rng);
    }

    Status Validate() const {
        if (node_id.empty()) {
            return Status::InvalidArg("node_id must not be empty");
        }
        if (listen_port == 0) {
            return Status::InvalidArg("listen_port must be non-zero");
        }
        if (raft_port == 0) {
            return Status::InvalidArg("raft_port must be non-zero");
        }
        if (raft_port == listen_port) {
            return Status::InvalidArg("raft_port must differ from listen_port");
        }
        if (raft_heartbeat_ms <= 0) {
            return Status::InvalidArg("raft_heartbeat_ms must be positive");
        }
        if (raft_election_max_ms < raft_election_min_ms) {
            return Status::InvalidArg(
                "raft_election_max_ms must be >= raft_election_min_ms");
        }
        if (raft_election_min_ms <= raft_heartbeat_ms * 3) {
            return Status::InvalidArg(
                "raft_election_min_ms must be > heartbeat_ms * 3");
        }
        if (raft_rpc_timeout_ms <= 0) {
            return Status::InvalidArg("raft_rpc_timeout_ms must be positive");
        }
        if (shard_count < 1) {
            return Status::InvalidArg("shard_count must be >= 1");
        }
        if (replica_count < 1) {
            return Status::InvalidArg("replica_count must be >= 1");
        }
        if (snapshot_entries == 0) {
            return Status::InvalidArg("snapshot_entries must be > 0");
        }
        if (max_log_entries == 0) {
            return Status::InvalidArg("max_log_entries must be > 0");
        }
        return Status::OK();
    }

    // Helper to parse "host:port" into a NodeAddr
    static NodeAddr ParsePeerAddr(const std::string& addr_str) {
        NodeAddr addr;
        size_t colon = addr_str.find(':');
        if (colon != std::string::npos) {
            addr.host = addr_str.substr(0, colon);
            addr.client_port = static_cast<uint16_t>(
                std::stoul(addr_str.substr(colon + 1)));
        } else {
            addr.host = addr_str;
        }
        return addr;
    }
};

// ============================================================
// LoadConfig — load from YAML file, applying defaults
// ============================================================
Status LoadConfig(const std::string& path, KvConfig& cfg);

} // namespace kvstore
} // namespace zero
