#pragma once
#include "kvstore/raft/raft_types.h"
#include "kvstore/raft/raft_config.h"
#include <random>
namespace zero { namespace kvstore {
class RaftNode;
class ElectionManager {
public:
    ElectionManager();
    void reset(RaftConfig& cfg, std::mt19937& rng);
    bool timeoutElapsed(int64_t elapsed_ms);
    void resetTimer();
    int64_t remaining() const { return timer_ms_; }
private:
    int64_t timer_ms_ = 0;
};
}} // namespace
