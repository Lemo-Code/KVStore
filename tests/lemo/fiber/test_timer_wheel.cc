/**
 * @file test_timer_wheel.cc
 * @brief 四层时间轮：循环定时、refresh/reset、长延迟、条件定时器
 */
#include "test_common.h"

#include "lemo/fiber/module.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <thread>

namespace {

void wait_until(const std::function<bool()>& pred, int max_iters = 300,
                int sleep_ms = 10) {
  for (int i = 0; i < max_iters && !pred(); ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
  }
}

void test_recurring_timer() {
  lemo::fiber::Scheduler::ptr sch(
      new lemo::fiber::Scheduler(1, false, "test_recurring"));
  sch->start();

  std::atomic<int> count{0};
  sch->addTimerPtr(30, [&count]() { count.fetch_add(1); }, true);

  wait_until([&count]() { return count.load() >= 3; }, 400, 20);
  LEMO_CHECK(count.load() >= 3);

  sch->stop();
}

void test_refresh_reset() {
  lemo::fiber::Scheduler::ptr sch(
      new lemo::fiber::Scheduler(1, false, "test_refresh_reset"));
  sch->start();

  std::atomic<int> fired{0};
  lemo::fiber::Timer::ptr timer =
      sch->addTimerPtr(200, [&fired]() { fired.fetch_add(1); });

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  LEMO_CHECK(timer->refresh());

  std::this_thread::sleep_for(std::chrono::milliseconds(120));
  LEMO_CHECK(fired.load() == 0);

  LEMO_CHECK(timer->reset(40, true));

  wait_until([&fired]() { return fired.load() >= 1; });
  LEMO_CHECK(fired.load() == 1);

  sch->stop();
}

void test_long_delay() {
  lemo::fiber::Scheduler::ptr sch(
      new lemo::fiber::Scheduler(1, false, "test_long_delay"));
  sch->start();

  std::atomic<int> fired{0};
  sch->addTimer(600, [&fired]() { fired.store(1); });

  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  LEMO_CHECK(fired.load() == 0);

  wait_until([&fired]() { return fired.load() == 1; }, 400, 20);
  LEMO_CHECK(fired.load() == 1);

  sch->stop();
}

void test_condition_timer() {
  lemo::fiber::Scheduler::ptr sch(
      new lemo::fiber::Scheduler(1, false, "test_cond_timer"));
  sch->start();

  std::atomic<int> fired{0};
  {
    auto token = std::make_shared<int>(0);
    std::shared_ptr<void> guard(token, static_cast<void*>(nullptr));
    std::weak_ptr<void> weak_guard(guard);
    sch->addConditionTimer(
        50, [&fired]() { fired.store(1); }, weak_guard, false);
    wait_until([&fired]() { return fired.load() == 1; });
    LEMO_CHECK(fired.load() == 1);
  }

  fired.store(0);
  auto token = std::make_shared<int>(0);
  std::shared_ptr<void> guard(token, static_cast<void*>(nullptr));
  std::weak_ptr<void> weak_guard(guard);
  guard.reset();
  token.reset();
  sch->addConditionTimer(
      50, [&fired]() { fired.store(1); }, weak_guard, false);

  std::this_thread::sleep_for(std::chrono::milliseconds(120));
  LEMO_CHECK(fired.load() == 0);

  sch->stop();
}

void test_timer_ptr_cancel() {
  lemo::fiber::Scheduler::ptr sch(
      new lemo::fiber::Scheduler(1, false, "test_timer_ptr_cancel"));
  sch->start();

  std::atomic<int> fired{0};
  lemo::fiber::Timer::ptr timer =
      sch->addTimerPtr(80, [&fired]() { fired.store(1); });
  LEMO_CHECK(timer->cancel());

  std::this_thread::sleep_for(std::chrono::milliseconds(150));
  LEMO_CHECK(fired.load() == 0);

  sch->stop();
}

}  // namespace

int main() {
  test_recurring_timer();
  test_refresh_reset();
  test_long_delay();
  test_condition_timer();
  test_timer_ptr_cancel();
  std::printf("PASS test_timer_wheel\n");
  return 0;
}
