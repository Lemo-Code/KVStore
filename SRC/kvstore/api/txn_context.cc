#include "kvstore/api/txn_context.h"
namespace zero { namespace kvstore {
Status TxnContext::Begin() { active_ = true; reads_.clear(); writes_.clear(); return Status::OK(); }
Status TxnContext::Get(const Key& k, Value& v) { reads_.push_back({k, ""}); return Status::OK(); }
Status TxnContext::Put(const Key& k, const Value& v) { writes_.push_back({k, v}); return Status::OK(); }
Status TxnContext::Delete(const Key& k) { writes_.push_back({k, "", true}); return Status::OK(); }
Status TxnContext::Commit() { active_ = false; return Status::OK(); }
Status TxnContext::Rollback() { active_ = false; reads_.clear(); writes_.clear(); return Status::OK(); }
}} // namespace
