/**
 * @file test_timer.cc
 * @brief 定时器与 Fiber::SleepMs
 */
#include "test_common.h"

#include "lemo/fiber/module.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace {

void test_add_timer_callback() {
  lemo::fiber::Scheduler::ptr sch(
      new lemo::fiber::Scheduler(1, false, "test_timer_cb"));
  sch->start();

  std::atomic<int> fired{0};
  sch->addTimer(50, [&fired]() { fired.store(1); });

  for (int i = 0; i < 200 && fired.load() == 0; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  LEMO_CHECK(fired.load() == 1);

  sch->stop();
}

void test_fiber_sleep_ms() {
  lemo::fiber::Scheduler::ptr sch(
      new lemo::fiber::Scheduler(1, false, "test_fiber_sleep"));
  sch->start();

  std::atomic<int> phase{0};
  sch->schedule([&phase]() {
    phase.store(1);
    lemo::fiber::Fiber::SleepMs(60);
    phase.store(2);
  });

  for (int i = 0; i < 200 && phase.load() < 1; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  LEMO_CHECK(phase.load() == 1);

  for (int i = 0; i < 200 && phase.load() < 2; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  LEMO_CHECK(phase.load() == 2);

  sch->stop();
}

void test_cancel_timer() {
  lemo::fiber::Scheduler::ptr sch(
      new lemo::fiber::Scheduler(1, false, "test_cancel_timer"));
  sch->start();

  std::atomic<int> fired{0};
  const uint64_t id = sch->addTimer(80, [&fired]() { fired.store(1); });
  sch->cancelTimer(id);

  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  LEMO_CHECK(fired.load() == 0);

  sch->stop();
}

void test_multiple_timers() {
  lemo::fiber::Scheduler::ptr sch(
      new lemo::fiber::Scheduler(2, false, "test_multi_timer"));
  sch->start();

  std::atomic<int> sum{0};
  for (int i = 1; i <= 5; ++i) {
    sch->addTimer(static_cast<uint64_t>(i * 20),
                  [i, &sum]() { sum.fetch_add(i); });
  }

  for (int i = 0; i < 300 && sum.load() < 15; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  LEMO_CHECK(sum.load() == 15);

  sch->stop();
}

}  // namespace

int main() {
  test_add_timer_callback();
  test_fiber_sleep_ms();
  test_cancel_timer();
  test_multiple_timers();
  std::printf("PASS test_timer\n");
  return 0;
}
