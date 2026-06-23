#pragma once
#include <cstdint>
#include <string>
namespace zero { namespace kvstore {
struct ClientConfig { int64_t connect_timeout_ms = 3000; int64_t request_timeout_ms = 5000; int max_retries = 3; };
}} // namespace
