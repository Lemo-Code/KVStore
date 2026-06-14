#include "test_common.h"

#include "lemo/utils/thread_util.h"

#include <atomic>
#include <thread>

int main() {
  LEMO_CHECK(lemo::utils::GetThreadId() > 0);

  lemo::utils::SetThreadName("main_test");
  LEMO_CHECK(lemo::utils::GetThreadName() == "main_test");

  lemo::utils::SetThreadName("");
  LEMO_CHECK(lemo::utils::GetThreadName() == "unknown");

  std::atomic<bool> done{false};
  std::string worker_name;
  std::thread worker([&done, &worker_name]() {
    lemo::utils::SetThreadName("worker_a");
    worker_name = lemo::utils::GetThreadName();
    done = true;
  });
  worker.join();
  LEMO_CHECK(done.load());
  LEMO_CHECK(worker_name == "worker_a");
  LEMO_CHECK(lemo::utils::GetThreadName() == "unknown");

  std::printf("PASS test_utils_thread\n");
  return 0;
}
