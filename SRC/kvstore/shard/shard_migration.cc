#include "kvstore/shard/shard_migration.h"
namespace zero { namespace kvstore {
Status Shard_migration::Init() { return Status::OK(); }
ShardId Shard_migration::Lookup(const Key&) { return 0; }
}} // namespace
