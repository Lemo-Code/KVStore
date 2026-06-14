/**
 * @file test_fiber.cc
 * @brief 协程：创建、切换、状态、多协程轮询
 */
#include "test_common.h"

#include "lemo/fiber/module.h"
#include "lemo/fiber/fiber_id.h"
#include "lemo/thread/module.h"

#include <atomic>
#include <cstring>
#include <vector>

namespace {

void fiber_func_basic() {
  LEMO_CHECK(lemo::fiber::GetCurrentFiberId() > 0);
  lemo::fiber::Fiber::YieldToHold();
  LEMO_CHECK(lemo::fiber::GetCurrentFiberId() > 0);
  lemo::fiber::Fiber::YieldToHold();
}

void test_basic_fiber() {
  LEMO_CHECK(lemo::fiber::GetCurrentFiberId() == 0);

  lemo::fiber::Fiber::ptr main_fiber = lemo::fiber::Fiber::GetThis();
  LEMO_CHECK(main_fiber != nullptr);
  LEMO_CHECK(main_fiber->getId() == 0);

  lemo::fiber::Fiber::ptr fiber(new lemo::fiber::Fiber(fiber_func_basic));
  LEMO_CHECK(fiber->getId() > 0);

  fiber->swapIn();
  LEMO_CHECK(fiber->getState() == lemo::fiber::Fiber::HOLD);

  fiber->swapIn();
  LEMO_CHECK(fiber->getState() == lemo::fiber::Fiber::HOLD);

  fiber->swapIn();
  LEMO_CHECK(fiber->getState() == lemo::fiber::Fiber::TERM);
}

void fiber_func_state() {
  LEMO_CHECK(lemo::fiber::Fiber::GetThis()->getState() ==
             lemo::fiber::Fiber::EXEC);
  lemo::fiber::Fiber::YieldToHold();
  LEMO_CHECK(lemo::fiber::Fiber::GetThis()->getState() ==
             lemo::fiber::Fiber::EXEC);
}

void test_fiber_state() {
  lemo::fiber::Fiber::ptr fiber(new lemo::fiber::Fiber(fiber_func_state));
  LEMO_CHECK(fiber->getState() == lemo::fiber::Fiber::INIT);

  fiber->swapIn();
  LEMO_CHECK(fiber->getState() == lemo::fiber::Fiber::HOLD);

  fiber->swapIn();
  LEMO_CHECK(fiber->getState() == lemo::fiber::Fiber::TERM);
}

void fiber_func_tls(int id) {
  LEMO_CHECK(lemo::fiber::GetCurrentFiberId() > 0);
  for (int i = 0; i < 3; ++i) {
    (void)id;
    lemo::fiber::Fiber::YieldToHold();
  }
}

void test_multiple_fibers() {
  std::vector<lemo::fiber::Fiber::ptr> fibers;
  for (int i = 0; i < 3; ++i) {
    fibers.push_back(lemo::fiber::Fiber::ptr(
        new lemo::fiber::Fiber(std::bind(fiber_func_tls, i))));
  }

  bool all_done = false;
  while (!all_done) {
    all_done = true;
    for (size_t i = 0; i < fibers.size(); ++i) {
      if (fibers[i]->getState() != lemo::fiber::Fiber::TERM &&
          fibers[i]->getState() != lemo::fiber::Fiber::EXCEPT) {
        all_done = false;
        fibers[i]->swapIn();
      }
    }
  }
}

void thread_fiber_test() {
  lemo::fiber::Fiber::ptr fiber(new lemo::fiber::Fiber([]() {
    LEMO_CHECK(lemo::fiber::GetCurrentFiberId() > 0);
    lemo::fiber::Fiber::YieldToHold();
    LEMO_CHECK(lemo::fiber::GetCurrentFiberId() > 0);
  }));
  fiber->swapIn();
  fiber->swapIn();
}

void test_fiber_in_threads() {
  std::vector<lemo::thread::Thread::ptr> threads;
  for (int i = 0; i < 3; ++i) {
    threads.push_back(lemo::thread::Thread::ptr(new lemo::thread::Thread(
        thread_fiber_test, "fiber_thread_" + std::to_string(i))));
  }
  for (size_t i = 0; i < threads.size(); ++i) {
    threads[i]->join();
  }
}

void test_fiber_stack() {
  lemo::fiber::Fiber::ptr fiber(new lemo::fiber::Fiber([]() {
    char buffer[1024];
    std::memset(buffer, 0, sizeof(buffer));
    LEMO_CHECK(lemo::fiber::Fiber::GetThis()->getId() > 0);
    (void)buffer;
  }));
  fiber->swapIn();
  LEMO_CHECK(fiber->getState() == lemo::fiber::Fiber::TERM);
}

void test_fiber_total_count() {
  const uint64_t before = lemo::fiber::Fiber::GetTotalCount();
  {
    lemo::fiber::Fiber::ptr fiber(new lemo::fiber::Fiber([]() {}));
    LEMO_CHECK(lemo::fiber::Fiber::GetTotalCount() == before + 1);
    fiber->swapIn();
  }
  LEMO_CHECK(lemo::fiber::Fiber::GetTotalCount() == before);
}

}  // namespace

int main() {
  test_basic_fiber();
  test_fiber_state();
  test_multiple_fibers();
  test_fiber_in_threads();
  test_fiber_stack();
  test_fiber_total_count();
  std::printf("PASS test_fiber\n");
  return 0;
}
