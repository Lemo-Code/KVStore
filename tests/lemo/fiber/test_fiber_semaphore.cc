/**
 * @file test_fiber_semaphore.cc
 */
#include "test_common.h"
#include "../io/test_io_common.h"

#include "lemo/fiber/fiber.h"
#include "lemo/fiber/sync/fiber_semaphore.h"
#include "lemo/io/iomanager.h"

#include <atomic>

namespace {

using lemo::fiber::sync::FiberSemaphore;

void test_sem_wait_notify() {
  lemo::io::IOManager iom(2, false, "test_fiber_sem");
  FiberSemaphore sem(0);
  std::atomic<int> done{0};

  iom.schedule([&sem, &done]() {
    sem.wait();
    done.store(1);
  });

  iom.schedule([&sem]() {
    lemo::fiber::Fiber::SleepMs(50);
    sem.notify();
  });

  LEMO_CHECK(lemo_io_test::wait_eq(done, 1, 3000));
  iom.stop();
}

void test_sem_try_wait() {
  lemo::io::IOManager iom(1, false, "test_sem_try");
  FiberSemaphore sem(1);
  std::atomic<int> ok{0};

  iom.schedule([&sem, &ok]() {
    LEMO_CHECK(sem.tryWait());
    LEMO_CHECK(!sem.tryWait());
    sem.notify();
    LEMO_CHECK(sem.tryWait());
    ok.store(1);
  });

  LEMO_CHECK(lemo_io_test::wait_eq(ok, 1, 3000));
  iom.stop();
}

}  // namespace

int main() {
  test_sem_wait_notify();
  test_sem_try_wait();
  std::printf("PASS test_fiber_semaphore\n");
  return 0;
}
