#include "kvstore/storage/sstable_reader.h"
#include <fstream>
#include <cstring>
namespace zero { namespace kvstore {
Status SSTableReader::Open(const std::string& path) {
    path_ = path;
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return Status::IOError("cannot open sstable: "+path);
    size_t sz = in.tellg(); data_.resize(sz); in.seekg(0); in.read(&data_[0], sz); in.close();
    if (sz < kSSTableFooterSize) return Status::IOError("sstable too small");
    if (!SSTableFooter::Decode(&data_[sz - kSSTableFooterSize], footer_)) return Status::IOError("bad sstable magic");
    const char* p = &data_[footer_.index_offset];
    for (uint32_t i = 0; i < footer_.data_block_count; ++i) {
        IndexEntry ie; uint32_t kl; memcpy(&kl, p, 4); p += 4;
        ie.last_key.assign(p, kl); p += kl;
        memcpy(&ie.block_offset, p, 8); p += 8;
        memcpy(&ie.block_size, p, 8); p += 8;
        memcpy(&ie.key_count, p, 4); p += 4;
        index_.push_back(ie);
    }
    return Status::OK();
}
ssize_t SSTableReader::BinarySearchIndex(const Key& k) {
    ssize_t lo = 0, hi = index_.size() - 1;
    while (lo <= hi) {
        ssize_t mid = (lo + hi) / 2;
        if (index_[mid].last_key < k) lo = mid + 1;
        else if (mid > 0 && index_[mid-1].last_key >= k) hi = mid - 1;
        else return mid;
    }
    return -1;
}
Status SSTableReader::Get(const Key& k, Value& v) {
    ssize_t idx = BinarySearchIndex(k);
    if (idx < 0) return Status::NotFound(k);
    const IndexEntry& ie = index_[idx];
    const char* p = &data_[ie.block_offset];
    const char* end = p + ie.block_size;
    while (p < end) {
        uint32_t kl, vl; memcpy(&kl, p, 4); p += 4; Key rk(p, kl); p += kl;
        memcpy(&vl, p, 4); p += 4; Value rv(p, vl); p += vl;
        if (rk == k) { v = rv; return Status::OK(); }
        if (rk > k) break;
    }
    return Status::NotFound(k);
}
Status SSTableReader::Scan(const Key& start, const Key& limit, size_t max, std::vector<KeyValue>& results) {
    ssize_t idx = BinarySearchIndex(start);
    if (idx < 0) return Status::OK();
    for (size_t b = idx; b < index_.size() && results.size() < max; ++b) {
        const IndexEntry& ie = index_[b];
        const char* p = &data_[ie.block_offset], *end = p + ie.block_size;
        while (p < end && results.size() < max) {
            uint32_t kl, vl; memcpy(&kl, p, 4); p += 4; Key rk(p, kl); p += kl;
            memcpy(&vl, p, 4); p += 4; Value rv(p, vl); p += vl;
            if (rk < start) continue;
            if (!limit.empty() && rk >= limit) return Status::OK();
            results.emplace_back(rk, rv);
        }
    }
    return Status::OK();
}
}} // namespace
