#pragma once
#include <cstdint>
#include <cstdlib>
namespace zero { namespace kvstore {
struct RaftConfig {
    int64_t heartbeat_ms       = 100;
    int64_t election_min_ms    = 500;
    int64_t election_max_ms    = 1000;
    int64_t rpc_timeout_ms     = 300;
    size_t  max_entries_append = 100;
    size_t  max_pending        = 500;
    size_t  snapshot_interval  = 10000;
    int64_t RandomizedElectionTimeout() const {
        return election_min_ms + (rand() % (election_max_ms - election_min_ms + 1));
    }
};
}} // namespace
