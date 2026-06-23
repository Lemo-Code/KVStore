#pragma once
#include <cstdint>
#include <string>
#include <cstring>
namespace zero { namespace kvstore {

// SSTable v1 format:
// [Data Block 0]...[Data Block N]
// [Index Block]
// [Footer: 40 bytes]
//   - magic: "KVSST01\0" (8 bytes)  
//   - index_offset (8 bytes LE)
//   - index_size (8 bytes LE)
//   - data_block_count (4 bytes LE)
//   - key_count (8 bytes LE)
//   - crc32c (4 bytes)

constexpr uint64_t kSSTableMagic = 0x3130545353564B; // "KVSST01\0" little-endian
constexpr size_t   kSSTableFooterSize = 40;
constexpr size_t   kDefaultBlockSize = 4096;

struct SSTableFooter {
    uint64_t magic = kSSTableMagic;
    uint64_t index_offset = 0;
    uint64_t index_size = 0;
    uint32_t data_block_count = 0;
    uint64_t key_count = 0;
    uint32_t crc32c = 0;

    void Encode(std::string& out) const {
        auto w64 = [&](uint64_t v) { out.append((const char*)&v, 8); };
        auto w32 = [&](uint32_t v) { out.append((const char*)&v, 4); };
        w64(magic); w64(index_offset); w64(index_size);
        w32(data_block_count); w64(key_count); w32(crc32c);
    }
    static bool Decode(const char* d, SSTableFooter& f) {
        auto r64 = [](const char*& p) -> uint64_t { uint64_t v; memcpy(&v, p, 8); p+=8; return v; };
        auto r32 = [](const char*& p) -> uint32_t { uint32_t v; memcpy(&v, p, 4); p+=4; return v; };
        const char* p = d;
        f.magic = r64(p); f.index_offset = r64(p); f.index_size = r64(p);
        f.data_block_count = r32(p); f.key_count = r64(p); f.crc32c = r32(p);
        return f.magic == kSSTableMagic;
    }
};

struct IndexEntry {
    std::string last_key;
    uint64_t block_offset = 0;
    uint64_t block_size = 0;
    uint32_t key_count = 0;
};

struct DataBlock {
    std::string data;
    std::string first_key;
    std::string last_key;
    uint32_t key_count = 0;

    void Clear() { data.clear(); first_key.clear(); last_key.clear(); key_count = 0; }
    void Add(const std::string& k, const std::string& v) {
        if (key_count == 0) first_key = k;
        last_key = k;
        uint32_t kl = k.size(), vl = v.size();
        data.append((const char*)&kl, 4); data.append(k);
        data.append((const char*)&vl, 4); data.append(v);
        key_count++;
    }
    bool Full() const { return data.size() >= kDefaultBlockSize; }
    bool Empty() const { return key_count == 0; }
};

}} // namespace
