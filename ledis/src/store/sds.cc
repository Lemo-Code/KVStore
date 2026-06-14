#include "ledis/store/sds.h"

#include <cstring>

namespace ledis {

Sds::Sds(const char* data, size_t len) {
  if (data && len > 0) {
    data_.assign(data, len);
  }
}

Sds::Sds(const char* s) {
  if (s) {
    data_.assign(s);
  }
}

Sds::Sds(std::string s) : data_(std::move(s)) {}

bool Sds::operator==(const Sds& other) const {
  return data_.size() == other.data_.size() &&
         (data_.empty() ||
          std::memcmp(data_.data(), other.data_.data(), data_.size()) == 0);
}

uint64_t Sds::hash() const {
  uint64_t h = 1469598103934665603ull;
  for (unsigned char c : data_) {
    h ^= c;
    h *= 1099511628211ull;
  }
  return h;
}

}  // namespace ledis
