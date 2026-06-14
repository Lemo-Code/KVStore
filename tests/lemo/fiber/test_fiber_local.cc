/**
 * @file test_fiber_local.cc
 */
#include "test_common.h"
#include "../io/test_io_common.h"

#include "lemo/fiber/fiber.h"
#include "lemo/fiber/sync/fiber_local.h"
#include "lemo/io/iomanager.h"

#include <atomic>
#include <string>

namespace {

using lemo::fiber::sync::FiberLocal;

void test_fiber_local_isolated() {
  lemo::io::IOManager iom(2, false, "test_fiber_local");
  FiberLocal<int> counter;
  std::atomic<int> done{0};

  iom.schedule([&counter, &done]() {
    counter.get() = 10;
    LEMO_CHECK(counter.get() == 10);
    done.fetch_add(1);
  });

  iom.schedule([&counter, &done]() {
    counter.get() = 20;
    LEMO_CHECK(counter.get() == 20);
    done.fetch_add(1);
  });

  LEMO_CHECK(lemo_io_test::wait_eq(done, 2, 3000));
  iom.stop();
}

void test_fiber_local_string() {
  lemo::io::IOManager iom(1, false, "test_fl_str");
  FiberLocal<std::string> name;
  std::atomic<int> ok{0};

  iom.schedule([&name, &ok]() {
    name.get() = "lemo";
    LEMO_CHECK(name.get() == "lemo");
    name.reset();
    name.get() = "nettycore";
    LEMO_CHECK(name.get() == "nettycore");
    ok.store(1);
  });

  LEMO_CHECK(lemo_io_test::wait_eq(ok, 1, 3000));
  iom.stop();
}

}  // namespace

int main() {
  test_fiber_local_isolated();
  test_fiber_local_string();
  std::printf("PASS test_fiber_local\n");
  return 0;
}
