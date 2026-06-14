#pragma once

#include "ledis/types.h"

namespace ledis {

enum class MaxmemoryPolicy {
  kAllkeysLru,
  kVolatileLru,
  kAllkeysLfu,
  kVolatileLfu,
};

MaxmemoryPolicy parseMaxmemoryPolicy(const String& value, bool* ok);
String maxmemoryPolicyName(MaxmemoryPolicy policy);
bool maxmemoryPolicyUsesLfu(MaxmemoryPolicy policy);
bool maxmemoryPolicyVolatileOnly(MaxmemoryPolicy policy);

}  // namespace ledis
