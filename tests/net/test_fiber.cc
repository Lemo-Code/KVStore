/**
 * @file test_fiber.cc
 * @brief 协程模块：创建、切换、状态、多协程轮询
 */
#include "test_common.h"

#include "common/util.h"
#include "fiber/module.h"

#include <atomic>
#include <cstring>
#include <vector>

namespace {

const char* FiberStateToString(net::Fiber::State state) {
  switch (state) {
    case net::Fiber::INIT:
      return "INIT";
    case net::Fiber::HOLD:
      return "HOLD";
    case net::Fiber::EXEC:
      return "EXEC";
    case net::Fiber::TERM:
      return "TERM";
    case net::Fiber::READY:
      return "READY";
    case net::Fiber::EXCEPT:
      return "EXCEPT";
    default:
      return "UNKNOWN";
  }
}

void fiber_func_basic() {
  NET_CHECK(net::GetFiberId() > 0);
  net::Fiber::YieldToHold();
  NET_CHECK(net::GetFiberId() > 0);
  net::Fiber::YieldToHold();
}

void test_basic_fiber() {
  NET_CHECK(net::GetFiberId() == 0);

  auto main_fiber = net::Fiber::GetThis();
  NET_CHECK(main_fiber != nullptr);
  NET_CHECK(main_fiber->getId() == 0);

  net::Fiber::ptr fiber(new net::Fiber(fiber_func_basic));
  NET_CHECK(fiber->getId() > 0);

  fiber->swapIn();
  NET_CHECK(fiber->getState() == net::Fiber::HOLD);

  fiber->swapIn();
  NET_CHECK(fiber->getState() == net::Fiber::HOLD);

  fiber->swapIn();
  NET_CHECK(fiber->getState() == net::Fiber::TERM);
}

void fiber_func_state() {
  NET_CHECK(net::Fiber::GetThis()->getState() == net::Fiber::EXEC);
  net::Fiber::YieldToHold();
  NET_CHECK(net::Fiber::GetThis()->getState() == net::Fiber::EXEC);
}

void test_fiber_state() {
  net::Fiber::ptr fiber(new net::Fiber(fiber_func_state));
  NET_CHECK(fiber->getState() == net::Fiber::INIT);

  fiber->swapIn();
  NET_CHECK(fiber->getState() == net::Fiber::HOLD);
  (void)FiberStateToString(fiber->getState());

  fiber->swapIn();
  NET_CHECK(fiber->getState() == net::Fiber::TERM);
}

void fiber_func_tls(int id) {
  NET_CHECK(net::GetFiberId() > 0);
  for (int i = 0; i < 3; ++i) {
    (void)id;
    net::Fiber::YieldToHold();
  }
}

void test_multiple_fibers() {
  std::vector<net::Fiber::ptr> fibers;
  for (int i = 0; i < 3; ++i) {
    fibers.push_back(
        net::Fiber::ptr(new net::Fiber(std::bind(fiber_func_tls, i))));
  }

  bool all_done = false;
  while (!all_done) {
    all_done = true;
    for (auto& fiber : fibers) {
      if (fiber->getState() != net::Fiber::TERM &&
          fiber->getState() != net::Fiber::EXCEPT) {
        all_done = false;
        fiber->swapIn();
      }
    }
  }
}

void thread_fiber_test() {
  auto fiber_func = []() {
    NET_CHECK(net::GetFiberId() > 0);
    net::Fiber::YieldToHold();
    NET_CHECK(net::GetFiberId() > 0);
  };

  net::Fiber::ptr fiber(new net::Fiber(fiber_func));
  fiber->swapIn();
  fiber->swapIn();
}

void test_fiber_in_threads() {
  std::vector<net::Thread::ptr> threads;
  for (int i = 0; i < 3; ++i) {
    threads.push_back(net::Thread::ptr(new net::Thread(
        thread_fiber_test, "fiber_thread_" + std::to_string(i))));
  }
  for (auto& thr : threads) {
    thr->join();
  }
}

void fiber_func_stack() {
  char buffer[1024];
  std::memset(buffer, 0, sizeof(buffer));
  NET_CHECK(net::Fiber::GetThis()->getId() > 0);
  (void)buffer;
}

void test_fiber_stack() {
  net::Fiber::ptr fiber(new net::Fiber(fiber_func_stack));
  fiber->swapIn();
  NET_CHECK(fiber->getState() == net::Fiber::TERM);
}

void test_fiber_total_count() {
  const uint64_t before = net::Fiber::GetTotalCount();
  {
    net::Fiber::ptr fiber(new net::Fiber([]() {}));
    NET_CHECK(net::Fiber::GetTotalCount() == before + 1);
    fiber->swapIn();
  }
  NET_CHECK(net::Fiber::GetTotalCount() == before);
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
