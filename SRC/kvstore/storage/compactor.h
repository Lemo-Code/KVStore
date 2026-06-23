#pragma once
#include "kvstore/common/kv_error.h"
#include <string>
#include <vector>
namespace zero { namespace kvstore {
class Compactor {
public:
    Status Compact(const std::string& data_dir, int level);
    Status MergeLevels(int src_level, int dst_level);
};
}} // namespace
