#include "kvstore/shard/shard_registry.h"
namespace zero { namespace kvstore {
Status Shard_registry::Init() { return Status::OK(); }
ShardId Shard_registry::Lookup(const Key&) { return 0; }
}} // namespace
