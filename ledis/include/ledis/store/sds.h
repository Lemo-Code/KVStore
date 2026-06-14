#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace ledis {

/** 二进制安全字符串；内部存储暂用 std::string，对外由 Sds 封装。 */
class Sds {
 public:
  Sds() = default;
  Sds(const char* data, size_t len);
  explicit Sds(const char* s);
  explicit Sds(std::string s);

  const char* data() const { return data_.data(); }
  size_t size() const { return data_.size(); }
  bool empty() const { return data_.empty(); }
  const std::string& str() const { return data_; }

  bool operator==(const Sds& other) const;
  bool operator!=(const Sds& other) const { return !(*this == other); }
  uint64_t hash() const;

 private:
  std::string data_;
};

}  // namespace ledis
