#pragma once
#include "kvstore/raft/raft_types.h"
#include "kvstore/protocol/kv_protocol.h"
#include "kvstore/common/kv_error.h"
#include <functional>
namespace zero { namespace kvstore {
Status SendRaftMessage(const NodeId& to, MsgType type, const std::string& body, std::string& reply);
Status RegisterTransportHandler(const NodeId& node_id, std::function<Status(const std::string& req, std::string& rsp)> handler);
Status SendVoteRequest(const NodeId& to, const RequestVoteReq& req, RequestVoteRsp& rsp);
Status SendAppendEntriesMsg(const NodeId& to, const AppendEntriesReq& req, AppendEntriesRsp& rsp);
}} // namespace
