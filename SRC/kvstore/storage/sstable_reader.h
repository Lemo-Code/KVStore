#pragma once
#include "kvstore/storage/sstable_format.h"
#include "kvstore/common/kv_types.h"
#include "kvstore/common/kv_error.h"
#include <vector>
#include <string>
#include <map>
namespace zero { namespace kvstore {
class SSTableReader {
public:
    Status Open(const std::string& path);
    Status Get(const Key& k, Value& v);
    Status Scan(const Key& start, const Key& limit, size_t max, std::vector<KeyValue>& results);
    const std::string& Path() const { return path_; }
    uint64_t KeyCount() const { return footer_.key_count; }
private:
    std::string ReadBlock(uint64_t offset, uint64_t size);
    ssize_t BinarySearchIndex(const Key& k);
    std::string path_;
    std::string data_; // memory-mapped (simplified: read whole file)
    SSTableFooter footer_;
    std::vector<IndexEntry> index_;
};
}} // namespace
