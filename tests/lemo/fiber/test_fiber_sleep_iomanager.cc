#include "test_common.h"

#include "lemo/fiber/module.h"
#include "lemo/io/module.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace {

bool wait_eq(const std::atomic<int>& v, int expected, int timeout_ms = 3000) {
  for (int i = 0; i < timeout_ms; i += 5) {
    if (v.load() == expected) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return v.load() == expected;
}

}  // namespace

int main() {
  lemo::io::IOManager::ptr iom(
      new lemo::io::IOManager(1, false, "test_fiber_sleep_iom"));
  std::atomic<int> phase{0};
  iom->schedule([&phase]() {
    phase.store(1);
    lemo::fiber::Fiber::SleepMs(60);
    phase.store(2);
  });

  LEMO_CHECK(wait_eq(phase, 1));
  LEMO_CHECK(wait_eq(phase, 2));
  iom->stop();
  std::printf("PASS test_fiber_sleep_iomanager\n");
  return 0;
}
