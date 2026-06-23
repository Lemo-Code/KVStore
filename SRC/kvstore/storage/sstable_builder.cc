#include "kvstore/storage/sstable_builder.h"
#include "kvstore/common/kv_utils.h"
#include <algorithm>
namespace zero { namespace kvstore {
Status SSTableBuilder::Build(const std::string& path, const std::vector<KeyValue>& data) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return Status::IOError("cannot create sstable: " + path);
    std::vector<IndexEntry> index;
    DataBlock blk;
    offset_ = 0;
    for (auto& kv : data) {
        if (blk.Full() && !blk.Empty()) {
            Status st = WriteBlock(blk, out, index);
            if (!st.ok()) return st;
            blk.Clear();
        }
        blk.Add(kv.key, kv.val);
    }
    if (!blk.Empty()) {
        Status st = WriteBlock(blk, out, index);
        if (!st.ok()) return st;
    }
    uint64_t index_offset = offset_;
    std::string index_data;
    for (auto& ie : index) {
        uint32_t kl = ie.last_key.size(); index_data.append((const char*)&kl, 4); index_data.append(ie.last_key);
        index_data.append((const char*)&ie.block_offset, 8);
        index_data.append((const char*)&ie.block_size, 8);
        index_data.append((const char*)&ie.key_count, 4);
    }
    out.write(index_data.data(), index_data.size());
    SSTableFooter footer; footer.index_offset = index_offset; footer.index_size = index_data.size();
    footer.data_block_count = static_cast<uint32_t>(index.size()); footer.key_count = data.size();
    std::string foot; footer.Encode(foot);
    out.write(foot.data(), foot.size());
    out.close();
    return Status::OK();
}
Status SSTableBuilder::WriteBlock(const DataBlock& blk, std::ofstream& out, std::vector<IndexEntry>& idx) {
    IndexEntry ie; ie.last_key = blk.last_key; ie.block_offset = offset_; ie.block_size = blk.data.size(); ie.key_count = blk.key_count;
    out.write(blk.data.data(), blk.data.size());
    offset_ += blk.data.size();
    idx.push_back(ie);
    return Status::OK();
}
}} // namespace
