#ifndef LSTL_LSM_BLOOM_FILTER_H
#define LSTL_LSM_BLOOM_FILTER_H

#include "memory.h"
#include "sequence/vector.h"

#include <cmath>

namespace lstl {
namespace lsm {

class BloomFilter {
 public:
  BloomFilter() : bits_(), bit_count_(0), hash_count_(0) {}

  void reset() {
    bits_.clear();
    bit_count_ = 0;
    hash_count_ = 0;
  }

  void init(size_t expected_entries, double false_positive_rate = 0.01) {
    if (expected_entries == 0) {
      expected_entries = 1;
    }
    if (false_positive_rate <= 0.0) {
      false_positive_rate = 0.01;
    }
    const double m =
        -static_cast<double>(expected_entries) * std::log(false_positive_rate) /
        (0.480453013918201 * 0.480453013918201);
    bit_count_ = static_cast<size_t>(m);
    if (bit_count_ < 64) {
      bit_count_ = 64;
    }
    hash_count_ = static_cast<size_t>(0.7 * static_cast<double>(bit_count_) /
                                      static_cast<double>(expected_entries));
    if (hash_count_ < 1) {
      hash_count_ = 1;
    }
    if (hash_count_ > 32) {
      hash_count_ = 32;
    }
    const size_t byte_count = (bit_count_ + 7) / 8;
    bits_.assign(byte_count, static_cast<unsigned char>(0));
    bit_count_ = byte_count * 8;
  }

  void add_bytes(const void* key, size_t key_len) {
    const size_t h1 = hash_bytes(key, key_len, 0x9e3779b9u);
    const size_t h2 = hash_bytes(key, key_len, 0x85ebca6bu);
    for (size_t i = 0; i < hash_count_; ++i) {
      const size_t pos = (h1 + i * h2) % bit_count_;
      bits_[pos / 8] |= static_cast<unsigned char>(1u << (pos % 8));
    }
  }

  bool may_contain_bytes(const void* key, size_t key_len) const {
    if (bit_count_ == 0) {
      return true;
    }
    const size_t h1 = hash_bytes(key, key_len, 0x9e3779b9u);
    const size_t h2 = hash_bytes(key, key_len, 0x85ebca6bu);
    for (size_t i = 0; i < hash_count_; ++i) {
      const size_t pos = (h1 + i * h2) % bit_count_;
      if ((bits_[pos / 8] & static_cast<unsigned char>(1u << (pos % 8))) == 0) {
        return false;
      }
    }
    return true;
  }

  template <typename Key>
  void add(const Key& key) {
    add_bytes(&key, sizeof(Key));
  }

  template <typename Key>
  bool may_contain(const Key& key) const {
    return may_contain_bytes(&key, sizeof(Key));
  }

  size_t bit_count() const { return bit_count_; }
  size_t hash_count() const { return hash_count_; }
  const unsigned char* data() const { return bits_.empty() ? 0 : &bits_[0]; }
  size_t data_size() const { return bits_.size(); }

  void load(const unsigned char* data, size_t data_size, size_t bit_count, size_t hash_count) {
    bit_count_ = bit_count;
    hash_count_ = hash_count;
    bits_.assign(data, data + data_size);
  }

 private:
  static size_t hash_bytes(const void* key, size_t key_len, size_t seed) {
    const unsigned char* p = static_cast<const unsigned char*>(key);
    size_t h = seed;
    for (size_t i = 0; i < key_len; ++i) {
      h ^= static_cast<size_t>(p[i]);
      h *= 0x9e3779b97f4a7c15ULL;
      h ^= h >> 32;
    }
    return h;
  }

  lstl::vector<unsigned char> bits_;
  size_t bit_count_;
  size_t hash_count_;
};

}  // namespace lsm
}  // namespace lstl

#endif  // LSTL_LSM_BLOOM_FILTER_H
