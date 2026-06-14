#pragma once

#include <cstddef>

namespace ledis {

struct ProtocolLimits {
  size_t query_buffer_limit = 1024 * 1024;
  size_t bulk_string_max = 512ULL * 1024 * 1024;
  size_t argc_max = 1024 * 1024;
};

}  // namespace ledis
