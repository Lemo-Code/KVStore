#include "kvstore/raft/raft_election.h"
#include "kvstore/raft/raft_node.h"
namespace zero { namespace kvstore {
ElectionManager::ElectionManager() : timer_ms_(500) {}
void ElectionManager::reset(RaftConfig& cfg, std::mt19937& rng) {
    std::uniform_int_distribution<int64_t> dist(cfg.election_min_ms, cfg.election_max_ms);
    timer_ms_ = dist(rng);
}
bool ElectionManager::timeoutElapsed(int64_t ms) { timer_ms_ -= ms; return timer_ms_ <= 0; }
void ElectionManager::resetTimer() { timer_ms_ = 500; }
}} // namespace
