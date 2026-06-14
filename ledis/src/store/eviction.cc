#include "ledis/store/eviction.h"

#include <cctype>

namespace ledis {

MaxmemoryPolicy parseMaxmemoryPolicy(const String& value_raw, bool* ok) {
  String value = value_raw;
  for (size_t i = 0; i < value.size(); ++i) {
    value[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(value[i])));
  }
  for (size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '_') {
      value[i] = '-';
    }
  }
  if (value == "allkeys-lru") {
    if (ok) {
      *ok = true;
    }
    return MaxmemoryPolicy::kAllkeysLru;
  }
  if (value == "volatile-lru") {
    if (ok) {
      *ok = true;
    }
    return MaxmemoryPolicy::kVolatileLru;
  }
  if (value == "allkeys-lfu") {
    if (ok) {
      *ok = true;
    }
    return MaxmemoryPolicy::kAllkeysLfu;
  }
  if (value == "volatile-lfu") {
    if (ok) {
      *ok = true;
    }
    return MaxmemoryPolicy::kVolatileLfu;
  }
  if (ok) {
    *ok = false;
  }
  return MaxmemoryPolicy::kAllkeysLru;
}

String maxmemoryPolicyName(MaxmemoryPolicy policy) {
  switch (policy) {
    case MaxmemoryPolicy::kVolatileLru:
      return String("volatile-lru");
    case MaxmemoryPolicy::kAllkeysLfu:
      return String("allkeys-lfu");
    case MaxmemoryPolicy::kVolatileLfu:
      return String("volatile-lfu");
    case MaxmemoryPolicy::kAllkeysLru:
    default:
      return String("allkeys-lru");
  }
}

bool maxmemoryPolicyUsesLfu(MaxmemoryPolicy policy) {
  return policy == MaxmemoryPolicy::kAllkeysLfu ||
         policy == MaxmemoryPolicy::kVolatileLfu;
}

bool maxmemoryPolicyVolatileOnly(MaxmemoryPolicy policy) {
  return policy == MaxmemoryPolicy::kVolatileLru ||
         policy == MaxmemoryPolicy::kVolatileLfu;
}

}  // namespace ledis
