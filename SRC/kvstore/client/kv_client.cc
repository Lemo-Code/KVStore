#include "kvstore/client/kv_client.h"
#include "kvstore/protocol/kv_codec.h"
#include "kvstore/protocol/kv_message.h"
#include "kvstore/common/kv_utils.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include <stdexcept>

namespace zero { namespace kvstore {

KvClient::KvClient() = default;

Status KvClient::Connect(const std::vector<std::string>& seeds) {
    seeds_ = seeds;
    for (auto& s : seeds_) {
        size_t colon = s.find(':');
        std::string host = (colon != std::string::npos) ? s.substr(0, colon) : s;
        int port = (colon != std::string::npos) ? std::stoi(s.substr(colon + 1)) : 9700;
        
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0) continue;
        
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
        
        struct timeval tv{3, 0}; // 3 second timeout
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        
        if (::connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            connections_[s] = fd;
        } else {
            close(fd);
        }
    }
    connected_ = !connections_.empty();
    return connected_ ? Status::OK() : Status::IOError("failed to connect to any seed");
}

Status KvClient::Get(const Key& k, Value& v) {
    if (connections_.empty()) return Status::IOError("not connected");
    int fd = connections_.begin()->second;
    KvGetReq req; req.key = k;
    std::string body; req.Serialize(body);
    std::string frame, reply;
    if (!EncodeFrame(MsgType::KV_GET, body, frame)) return Status::IOError("encode failed");
    if (!SendRecvFrame(fd, frame, reply)) return Status::Timeout("GET");
    KvGetRsp rsp;
    if (!rsp.Deserialize(reply.data(), reply.size())) return Status::IOError("deserialize");
    if (!rsp.found) return Status::NotFound(k);
    v = rsp.value;
    return Status::OK();
}

Status KvClient::Put(const Key& k, const Value& v) {
    if (connections_.empty()) return Status::IOError("not connected");
    int fd = connections_.begin()->second;
    KvPutReq req; req.key = k; req.value = v;
    std::string body; req.Serialize(body);
    std::string frame, reply;
    if (!EncodeFrame(MsgType::KV_PUT, body, frame)) return Status::IOError("encode failed");
    if (!SendRecvFrame(fd, frame, reply)) return Status::Timeout("PUT");
    KvPutRsp rsp;
    return (rsp.Deserialize(reply.data(), reply.size()) && rsp.ok) ? Status::OK() : Status::IOError("put failed");
}

Status KvClient::Delete(const Key& k) {
    if (connections_.empty()) return Status::IOError("not connected");
    int fd = connections_.begin()->second;
    KvDeleteReq req; req.key = k;
    std::string body; req.Serialize(body);
    std::string frame, reply;
    if (!EncodeFrame(MsgType::KV_DELETE, body, frame)) return Status::IOError("encode failed");
    if (!SendRecvFrame(fd, frame, reply)) return Status::Timeout("DEL");
    KvDeleteRsp rsp;
    return (rsp.Deserialize(reply.data(), reply.size()) && rsp.ok) ? Status::OK() : Status::IOError("delete failed");
}

Status KvClient::Scan(const Key& start, const Key& limit, size_t max, std::vector<KeyValue>& r) {
    if (connections_.empty()) return Status::IOError("not connected");
    int fd = connections_.begin()->second;
    KvScanReq req; req.start = start; req.limit = limit; req.max_count = max;
    std::string body; req.Serialize(body);
    std::string frame, reply;
    if (!EncodeFrame(MsgType::KV_SCAN, body, frame)) return Status::IOError("encode failed");
    if (!SendRecvFrame(fd, frame, reply)) return Status::Timeout("SCAN");
    KvScanRsp rsp;
    if (!rsp.Deserialize(reply.data(), reply.size())) return Status::IOError("deserialize scan");
    r = std::move(rsp.results);
    return Status::OK();
}

void KvClient::Close() {
    for (auto& [_, fd] : connections_) close(fd);
    connections_.clear(); connected_ = false;
}

bool KvClient::SendRecvFrame(int fd, const std::string& frame, std::string& reply) {
    ssize_t sent = send(fd, frame.data(), frame.size(), 0);
    if (sent != (ssize_t)frame.size()) return false;
    
    char buf[65536];
    ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return false;
    
    MsgType type;
    return DecodeFrame(buf, n, type, reply);
}

}} // namespace
