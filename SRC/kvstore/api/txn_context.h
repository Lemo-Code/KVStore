#pragma once
#include "kvstore/common/kv_types.h"
#include "kvstore/common/kv_error.h"
#include <vector>
namespace zero { namespace kvstore {
class TxnContext {
public:
    Status Begin();
    Status Get(const Key& k, Value& v);
    Status Put(const Key& k, const Value& v);
    Status Delete(const Key& k);
    Status Commit();
    Status Rollback();
private:
    bool active_ = false;
    std::vector<KeyValue> reads_;
    std::vector<KeyValue> writes_;
};
}} // namespace
