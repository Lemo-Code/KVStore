#pragma once
#include "kvstore/shard/shard_types.h"
#include "kvstore/common/kv_error.h"
#include <vector>
namespace zero { namespace kvstore {
class Shard_migration { public: Status Init(); ShardId Lookup(const Key& k); };
}} // namespace
