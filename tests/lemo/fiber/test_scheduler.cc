/**
 * @file test_scheduler.cc
 * @brief 协程调度器：任务投递、多线程、yield/ready、switchTo
 */
#include "test_common.h"

#include "lemo/fiber/fiber_id.h"
#include "lemo/fiber/module.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <vector>

namespace {

void test_scheduler_basic() {
  lemo::fiber::Scheduler::ptr sch(
      new lemo::fiber::Scheduler(2, false, "test_scheduler_basic"));
  sch->start();

  std::atomic<int> count{0};
  sch->schedule([&count]() { ++count; });
  sch->schedule([&count]() { ++count; });

  for (int i = 0; i < 100 && count.load() < 2; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  LEMO_CHECK(count.load() == 2);

  sch->stop();
}

void test_scheduler_yield_ready() {
  lemo::fiber::Scheduler::ptr sch(
      new lemo::fiber::Scheduler(1, false, "test_scheduler_yield"));
  sch->start();

  std::atomic<int> phase{0};
  sch->schedule([&phase]() {
    LEMO_CHECK(phase.load() == 0);
    phase.store(1);
    lemo::fiber::Fiber::YieldToReady();
    LEMO_CHECK(phase.load() == 1);
    phase.store(2);
  });

  for (int i = 0; i < 200 && phase.load() < 2; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  LEMO_CHECK(phase.load() == 2);

  sch->stop();
}

void test_scheduler_use_caller() {
  lemo::fiber::Scheduler sch(2, true, "test_scheduler_caller");
  sch.start();

  std::atomic<int> done{0};
  sch.schedule([&done]() { done.fetch_add(1); });
  sch.schedule([&done]() { done.fetch_add(1); });

  for (int i = 0; i < 200 && done.load() < 2; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  LEMO_CHECK(done.load() == 2);

  sch.stop();
}

void test_scheduler_batch_schedule() {
  lemo::fiber::Scheduler::ptr sch(
      new lemo::fiber::Scheduler(2, false, "test_scheduler_batch"));
  sch->start();

  std::atomic<int> sum{0};
  std::vector<std::function<void()>> tasks;
  for (int i = 1; i <= 10; ++i) {
    tasks.push_back([i, &sum]() { sum.fetch_add(i); });
  }
  sch->schedule(tasks.begin(), tasks.end());

  for (int i = 0; i < 200 && sum.load() < 55; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  LEMO_CHECK(sum.load() == 55);

  sch->stop();
}

void test_scheduler_switcher() {
  lemo::fiber::Scheduler::ptr sch_a(
      new lemo::fiber::Scheduler(1, false, "scheduler_a"));
  lemo::fiber::Scheduler::ptr sch_b(
      new lemo::fiber::Scheduler(1, false, "scheduler_b"));
  sch_a->start();
  sch_b->start();

  std::atomic<int> flag{0};
  sch_a->schedule([&]() {
    {
      lemo::fiber::SchedulerSwitcher switcher(sch_b.get());
      LEMO_CHECK(lemo::fiber::Scheduler::GetThis() == sch_b.get());
      flag.store(1);
    }
    LEMO_CHECK(lemo::fiber::Scheduler::GetThis() == sch_a.get());
    flag.store(2);
  });

  for (int i = 0; i < 200 && flag.load() < 2; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  LEMO_CHECK(flag.load() == 2);

  sch_a->stop();
  sch_b->stop();
}

void test_scheduler_fiber_id_in_task() {
  lemo::fiber::Scheduler::ptr sch(
      new lemo::fiber::Scheduler(1, false, "test_scheduler_fid"));
  sch->start();

  std::atomic<uint32_t> fiber_id{0};
  sch->schedule([&fiber_id]() {
    fiber_id.store(lemo::fiber::GetCurrentFiberId());
  });

  for (int i = 0; i < 100 && fiber_id.load() == 0; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  LEMO_CHECK(fiber_id.load() > 0);

  sch->stop();
}

void test_scheduler_work_stealing() {
  lemo::fiber::Scheduler::ptr sch(
      new lemo::fiber::Scheduler(4, false, "test_work_stealing"));
  sch->start();

  std::atomic<int> sum{0};
  for (int i = 1; i <= 100; ++i) {
    sch->schedule([i, &sum]() { sum.fetch_add(i); });
  }

  for (int i = 0; i < 500 && sum.load() < 5050; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  LEMO_CHECK(sum.load() == 5050);

  sch->stop();
}

void test_scheduler_runnext_depth() {
  lemo::fiber::Scheduler::ptr sch(
      new lemo::fiber::Scheduler(1, false, "test_runnext"));
  sch->start();

  std::atomic<int> depth{0};
  std::atomic<int> max_depth{0};
  sch->schedule([&]() {
    for (int i = 0; i < 8; ++i) {
      const int d = depth.fetch_add(1) + 1;
      int expected = max_depth.load();
      while (d > expected &&
             !max_depth.compare_exchange_weak(expected, d)) {
      }
      lemo::fiber::Fiber::YieldToReady();
      depth.fetch_sub(1);
    }
  });

  for (int i = 0; i < 200 && max_depth.load() < 1; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  LEMO_CHECK(max_depth.load() == 1);

  sch->stop();
}

}  // namespace

int main() {
  test_scheduler_basic();
  test_scheduler_yield_ready();
  test_scheduler_use_caller();
  test_scheduler_batch_schedule();
  test_scheduler_switcher();
  test_scheduler_fiber_id_in_task();
  test_scheduler_work_stealing();
  test_scheduler_runnext_depth();
  std::printf("PASS test_scheduler\n");
  return 0;
}
