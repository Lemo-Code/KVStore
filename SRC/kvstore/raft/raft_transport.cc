#include "kvstore/raft/raft_transport.h"
#include "kvstore/protocol/kv_codec.h"
#include "kvstore/common/kv_utils.h"
#include <map>
#include <mutex>
namespace zero { namespace kvstore {
// Simple in-process transport registry for testing
namespace {
    struct TransportEntry {
        std::function<Status(const std::string&, std::string&)> handler;
    };
    std::map<NodeId, TransportEntry> g_transport_map;
    std::mutex g_transport_mutex;
}
Status RegisterTransportHandler(const NodeId& node_id,
    std::function<Status(const std::string& req, std::string& rsp)> handler) {
    std::lock_guard<std::mutex> lock(g_transport_mutex);
    g_transport_map[node_id] = {std::move(handler)};
    return Status::OK();
}
Status SendRaftMessage(const NodeId& to, MsgType type, const std::string& body, std::string& reply) {
    std::lock_guard<std::mutex> lock(g_transport_mutex);
    auto it = g_transport_map.find(to);
    if (it == g_transport_map.end()) return Status::IOError("node not found: " + to);
    // Encode the Raft message as a frame
    std::string frame;
    if (!EncodeFrame(type, body, frame)) return Status::IOError("encode failed");
    return it->second.handler(frame, reply);
}
// Serialize and send any Raft message
Status SendVoteRequest(const NodeId& to, const RequestVoteReq& req, RequestVoteRsp& rsp) {
    std::string body; req.Serialize(body);
    std::string reply;
    Status st = SendRaftMessage(to, MsgType::RAFT_VOTE_REQ, body, reply);
    if (!st.ok()) return st;
    return RequestVoteRsp::Parse(reply.data(), reply.size(), rsp) ? Status::OK() : Status::IOError("parse vote rsp");
}
Status SendAppendEntriesMsg(const NodeId& to, const AppendEntriesReq& req, AppendEntriesRsp& rsp) {
    std::string body; req.Serialize(body);
    std::string reply;
    Status st = SendRaftMessage(to, MsgType::RAFT_APPEND_REQ, body, reply);
    if (!st.ok()) return st;
    return AppendEntriesRsp::Parse(reply.data(), reply.size(), rsp) ? Status::OK() : Status::IOError("parse append rsp");
}
}} // namespace
