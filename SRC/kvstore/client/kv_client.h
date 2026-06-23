#pragma once
#include "kvstore/common/kv_types.h"
#include "kvstore/common/kv_error.h"
#include <vector>
#include <string>
#include <map>
#include <atomic>
namespace zero { namespace kvstore {
class KvClient {
public:
    KvClient();
    ~KvClient() { Close(); }
    Status Connect(const std::vector<std::string>& seeds);
    Status Get(const Key& k, Value& v);
    Status Put(const Key& k, const Value& v);
    Status Delete(const Key& k);
    Status Scan(const Key& start, const Key& limit, size_t max, std::vector<KeyValue>& r);
    bool IsConnected() const { return connected_; }
    size_t NodeCount() const { return connections_.size(); }
    void Close();
private:
    bool SendRecvFrame(int fd, const std::string& frame, std::string& reply);
    std::vector<std::string> seeds_;
    std::map<std::string, int> connections_; // addr -> fd
    bool connected_ = false;
};
}} // namespace
