#include "lemo/utils/time_util.h"

#include <chrono>

namespace lemo {
namespace utils {
namespace {

uint64_t g_start_ms = 0;

struct ElapseInit {
  ElapseInit() { g_start_ms = NowMs(); }
};
ElapseInit g_elapse_init;

}  // namespace

uint64_t NowMs() {
  using Clock = std::chrono::steady_clock;
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          Clock::now().time_since_epoch())
          .count());
}

uint32_t GetElapse() {
  return static_cast<uint32_t>(NowMs() - g_start_ms);
}

}  // namespace utils
}  // namespace lemo
