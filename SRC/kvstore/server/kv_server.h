#pragma once
#include "kvstore/common/kv_types.h"
#include "kvstore/common/kv_error.h"
#include "kvstore/config/kv_config.h"
#include "kvstore/storage/memory_engine.h"
#include "kvstore/raft/raft_node.h"
#include "kvstore/shard/consistent_hash.h"
#include "kvstore/api/kv_api.h"
#include "kvstore/protocol/kv_message.h"
#include <memory>
#include <atomic>
#include <thread>
namespace zero { namespace kvstore {
class KvServer {
public:
    KvServer(const KvConfig& cfg);
    ~KvServer();
    Status Start();
    Status Stop();
    bool IsRunning() const { return running_; }
    Status HandleRequest(const std::string& body, std::string& reply);
    AdminStatusRsp GetStatus() const;
    KvApi& Api() { return api_; }
private:
    void AcceptLoop();
    void HandleClient(int fd);
    KvConfig cfg_;
    std::atomic<bool> running_{false};
    std::atomic<bool> listener_running_{false};
    std::unique_ptr<MemoryEngine> engine_;
    std::unique_ptr<RaftNode> raft_;
    ConsistentHash chash_;
    KvApi api_;
    std::vector<RaftPeer> peers_;
    int64_t start_time_ms_ = 0;
    std::thread listener_thread_;
};
}} // namespace
