/**
 * @file test_fiber_mutex.cc
 */
#include "test_common.h"
#include "../io/test_io_common.h"

#include "lemo/fiber/fiber.h"
#include "lemo/fiber/scheduler.h"
#include "lemo/fiber/sync/fiber_mutex.h"
#include "lemo/io/iomanager.h"

#include <atomic>

namespace {

using lemo::fiber::sync::FiberMutex;

void test_mutex_two_fibers() {
  lemo::io::IOManager iom(2, false, "test_fiber_mutex");
  FiberMutex mtx;
  std::atomic<int> holder{0};
  std::atomic<int> done_a{0};
  std::atomic<int> done_b{0};

  iom.schedule([&mtx, &holder, &done_a]() {
    mtx.lock();
    holder.store(1);
    lemo::fiber::Fiber::SleepMs(80);
    LEMO_CHECK(holder.load() == 1);
    holder.store(0);
    mtx.unlock();
    done_a.store(1);
  });

  iom.schedule([&mtx, &holder, &done_b]() {
    LEMO_CHECK(lemo_io_test::wait_eq(holder, 1, 3000));
    mtx.lock();
    LEMO_CHECK(holder.load() == 0);
    holder.store(2);
    mtx.unlock();
    done_b.store(1);
  });

  LEMO_CHECK(lemo_io_test::wait_eq(done_b, 1, 3000));
  LEMO_CHECK(lemo_io_test::wait_eq(done_a, 1, 3000));
  iom.stop();
}

void test_mutex_try_lock() {
  lemo::io::IOManager iom(1, false, "test_mutex_try");
  FiberMutex mtx;
  std::atomic<int> ok{0};

  iom.schedule([&mtx, &ok]() {
    LEMO_CHECK(mtx.try_lock());
    LEMO_CHECK(!mtx.try_lock());
    mtx.unlock();
    LEMO_CHECK(mtx.try_lock());
    mtx.unlock();
    ok.store(1);
  });

  LEMO_CHECK(lemo_io_test::wait_eq(ok, 1, 3000));
  iom.stop();
}

}  // namespace

int main() {
  test_mutex_two_fibers();
  test_mutex_try_lock();
  std::printf("PASS test_fiber_mutex\n");
  return 0;
}
