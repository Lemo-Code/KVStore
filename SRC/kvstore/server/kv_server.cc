#include "kvstore/server/kv_server.h"
#include "kvstore/common/kv_utils.h"
#include "kvstore/protocol/kv_codec.h"
#include "kvstore/protocol/kv_message.h"
#include "kvstore/raft/raft_transport.h"
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <algorithm>

namespace zero { namespace kvstore {

KvServer::KvServer(const KvConfig& cfg) : cfg_(cfg) {}
KvServer::~KvServer() { Stop(); }

// ---- Lifecycle ----
Status KvServer::Start() {
    engine_ = std::make_unique<MemoryEngine>();
    api_.SetEngine(engine_.get());

    RaftConfig rcfg; rcfg.heartbeat_ms = cfg_.raft_heartbeat_ms;
    rcfg.election_min_ms = cfg_.raft_election_min_ms;
    rcfg.election_max_ms = cfg_.raft_election_max_ms;
    rcfg.snapshot_interval = cfg_.snapshot_entries;

    for (auto& addr : cfg_.peer_addrs) {
        RaftPeer p; p.id = addr; p.addr.id = addr; peers_.push_back(p);
    }
    raft_ = std::make_unique<RaftNode>(rcfg, cfg_.node_id, peers_);
    raft_->SetEngine(engine_.get());

    // Register transport for this node
    RegisterTransportHandler(cfg_.node_id,
        [this](const std::string& req, std::string& rsp) {
            return HandleRequest(req, rsp);
        });

    // Set up Raft transport using the registered handler
    raft_->SetTransport([this](const NodeId& to, MsgType type, const std::string& body, std::string& reply) {
        return SendRaftMessage(to, type, body, reply);
    });

    for (auto& addr : cfg_.peer_addrs) chash_.AddNode(addr);

    // Start TCP listener on a background thread
    listener_running_ = true;
    listener_thread_ = std::thread([this]() { this->AcceptLoop(); });

    running_ = true; start_time_ms_ = NowMs();
    return Status::OK();
}

Status KvServer::Stop() {
    running_ = false; listener_running_ = false;
    if (listener_thread_.joinable()) listener_thread_.join();
    if (engine_) engine_->Close();
    return Status::OK();
}

// ---- TCP Accept Loop ----
void KvServer::AcceptLoop() {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) return;
    int opt = 1; setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons(cfg_.listen_port);
    addr.sin_addr.s_addr = inet_addr(cfg_.listen_host.c_str());
    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) { close(listen_fd); return; }
    if (listen(listen_fd, 128) < 0) { close(listen_fd); return; }

    struct timeval tv{1, 0}; // 1 second timeout
    while (listener_running_) {
        fd_set rfds; FD_ZERO(&rfds); FD_SET(listen_fd, &rfds);
        int ret = select(listen_fd + 1, &rfds, nullptr, nullptr, &tv);
        if (ret <= 0) continue;
        int client_fd = accept(listen_fd, nullptr, nullptr);
        if (client_fd < 0) continue;
        // Handle client in a simple loop
        HandleClient(client_fd);
        close(client_fd);
    }
    close(listen_fd);
}

void KvServer::HandleClient(int fd) {
    char buf[65536];
    std::string accumulated;
    struct timeval tv{5, 0};
    while (running_) {
        fd_set rfds; FD_ZERO(&rfds); FD_SET(fd, &rfds);
        if (select(fd + 1, &rfds, nullptr, nullptr, &tv) <= 0) break;
        ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
        if (n <= 0) break;
        accumulated.append(buf, n);

        // Process complete frames
        while (accumulated.size() >= kFrameHeaderSize) {
            uint32_t body_len;
            memcpy(&body_len, accumulated.data() + 6, 4);
            if (accumulated.size() < kFrameHeaderSize + body_len + 4) break; // incomplete
            std::string frame = accumulated.substr(0, kFrameHeaderSize + body_len + 4);
            accumulated.erase(0, frame.size());
            std::string reply;
            Status st = HandleRequest(frame, reply);
            if (st.ok() && !reply.empty()) send(fd, reply.data(), reply.size(), 0);
        }
    }
}

// ---- Request Handler ----
Status KvServer::HandleRequest(const std::string& body, std::string& reply) {
    MsgType type; std::string inner;
    if (!DecodeFrame(body.data(), body.size(), type, inner)) return Status::InvalidArg("bad frame");

    switch (type) {
        case MsgType::KV_GET: {
            KvGetReq req; KvGetRsp rsp;
            if (req.Deserialize(inner.data(), inner.size())) {
                Value v; Status st = api_.Get(req.key, v);
                rsp.found = st.ok(); rsp.value = v;
            }
            rsp.Serialize(reply);
            return EncodeFrame(MsgType::KV_GET, reply, reply) ? Status::OK() : Status::IOError("encode");
        }
        case MsgType::KV_PUT: {
            KvPutReq req; KvPutRsp rsp;
            if (req.Deserialize(inner.data(), inner.size())) {
                Status st = api_.Put(req.key, req.value); rsp.ok = st.ok();
            }
            rsp.Serialize(reply);
            return EncodeFrame(MsgType::KV_PUT, reply, reply) ? Status::OK() : Status::IOError("encode");
        }
        case MsgType::KV_DELETE: {
            KvDeleteReq req; KvDeleteRsp rsp;
            if (req.Deserialize(inner.data(), inner.size())) {
                Status st = api_.Delete(req.key); rsp.ok = st.ok();
            }
            rsp.Serialize(reply);
            return EncodeFrame(MsgType::KV_DELETE, reply, reply) ? Status::OK() : Status::IOError("encode");
        }
        case MsgType::KV_SCAN: {
            KvScanReq req; KvScanRsp rsp;
            if (req.Deserialize(inner.data(), inner.size())) {
                api_.Scan(req.start, req.limit, req.max_count, rsp.results);
            }
            rsp.Serialize(reply);
            return EncodeFrame(MsgType::KV_SCAN, reply, reply) ? Status::OK() : Status::IOError("encode");
        }
        case MsgType::KV_BATCH: {
            KvBatchReq req; KvBatchRsp rsp;
            if (req.Deserialize(inner.data(), inner.size())) {
                Status st = engine_->ApplyBatch(req.batch); rsp.ok = st.ok(); rsp.processed = static_cast<int32_t>(req.batch.size());
            }
            rsp.Serialize(reply);
            return EncodeFrame(MsgType::KV_BATCH, reply, reply) ? Status::OK() : Status::IOError("encode");
        }
        case MsgType::RAFT_VOTE_REQ: case MsgType::RAFT_VOTE_RSP:
        case MsgType::RAFT_APPEND_REQ: case MsgType::RAFT_APPEND_RSP:
        case MsgType::RAFT_SNAP_REQ: case MsgType::RAFT_SNAP_RSP:
            if (raft_) { raft_->Step(type, inner.data(), inner.size()); }
            reply = "OK"; return Status::OK();
        case MsgType::ADMIN_STATUS: {
            AdminStatusRsp rsp = GetStatus();
            rsp.Serialize(reply);
            return EncodeFrame(MsgType::ADMIN_STATUS, reply, reply) ? Status::OK() : Status::IOError("encode");
        }
        case MsgType::ADMIN_JOIN: {
            std::string node_id(inner.data(), inner.size());
            RaftPeer p; p.id = node_id; raft_->AddPeer(p);
            reply = "OK"; return Status::OK();
        }
        case MsgType::ADMIN_LEAVE: {
            std::string node_id(inner.data(), inner.size());
            raft_->RemovePeer(node_id);
            reply = "OK"; return Status::OK();
        }
        default: return Status::InvalidArg("unknown msg type");
    }
}

AdminStatusRsp KvServer::GetStatus() const {
    AdminStatusRsp rsp;
    rsp.node_id = cfg_.node_id;
    rsp.role = raft_ ? RoleName(raft_->Role()) : "unknown";
    rsp.term = raft_ ? raft_->CurrentTerm() : 0;
    rsp.leader_id = raft_ ? raft_->LeaderId() : "unknown";
    rsp.key_count = engine_ ? engine_->KeyCount() : 0;
    rsp.uptime_ms = NowMs() - start_time_ms_;
    return rsp;
}

}} // namespace
