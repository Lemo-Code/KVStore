#include "kvstore/api/kv_api.h"
namespace zero { namespace kvstore {
KvApi::KvApi() = default;
Status KvApi::Get(const Key& k, Value& v) { return engine_ ? engine_->Get(k, v) : Status::IOError("no engine"); }
Status KvApi::Put(const Key& k, const Value& v) { return engine_ ? engine_->Put(k, v) : Status::IOError("no engine"); }
Status KvApi::Delete(const Key& k) { return engine_ ? engine_->Delete(k) : Status::IOError("no engine"); }
Status KvApi::Scan(const Key& s, const Key& l, size_t m, std::vector<KeyValue>& r) { return engine_ ? engine_->Scan(s, l, m, r) : Status::IOError("no engine"); }
}} // namespace
