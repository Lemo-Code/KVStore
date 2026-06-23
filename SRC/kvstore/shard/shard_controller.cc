#include "kvstore/shard/shard_controller.h"
namespace zero { namespace kvstore {
Status Shard_controller::Init() { return Status::OK(); }
ShardId Shard_controller::Lookup(const Key&) { return 0; }
}} // namespace
