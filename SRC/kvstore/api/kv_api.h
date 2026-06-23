#pragma once
#include "kvstore/common/kv_types.h"
#include "kvstore/common/kv_error.h"
#include "kvstore/storage/storage_engine.h"
#include <memory>
namespace zero { namespace kvstore {
class KvApi {
public:
    KvApi();
    Status Get(const Key& k, Value& v);
    Status Put(const Key& k, const Value& v);
    Status Delete(const Key& k);
    Status Scan(const Key& start, const Key& limit, size_t max, std::vector<KeyValue>& r);
    void SetEngine(IKvEngine* e) { engine_ = e; }
private:
    IKvEngine* engine_ = nullptr;
};
}} // namespace
