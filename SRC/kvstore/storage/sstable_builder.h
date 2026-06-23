#pragma once
#include "kvstore/storage/sstable_format.h"
#include "kvstore/common/kv_types.h"
#include "kvstore/common/kv_types.h"
#include "kvstore/common/kv_error.h"
#include <vector>
#include <string>
#include <fstream>
namespace zero { namespace kvstore {
class SSTableBuilder {
public:
    Status Build(const std::string& path, const std::vector<KeyValue>& data);
private:
    Status WriteBlock(const DataBlock& blk, std::ofstream& out, std::vector<IndexEntry>& index);
    uint64_t offset_ = 0;
};
}} // namespace
