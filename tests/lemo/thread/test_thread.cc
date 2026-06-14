#include "test_common.h"

#include "lemo/thread/module.h"
#include "lemo/utils/thread_util.h"

#include <atomic>
#include <vector>

namespace {

void test_basic_thread() {
  std::atomic<int> done{0};
  std::vector<lemo::thread::Thread::ptr> threads;
  for (int i = 0; i < 3; ++i) {
    threads.push_back(lemo::thread::Thread::ptr(new lemo::thread::Thread(
        [&done]() { ++done; }, "worker_" + std::to_string(i))));
  }
  for (size_t i = 0; i < threads.size(); ++i) {
    LEMO_CHECK(lemo::thread::Thread::GetThis() == nullptr);
    threads[i]->join();
  }
  LEMO_CHECK(done.load() == 3);
}

lemo::thread::Mutex g_mutex;
int g_counter = 0;

void mutex_worker(int ops) {
  for (int i = 0; i < ops; ++i) {
    lemo::thread::Mutex::Lock lock(g_mutex);
    ++g_counter;
  }
}

void test_mutex() {
  g_counter = 0;
  const int kThreads = 4;
  const int kOps = 10000;
  std::vector<lemo::thread::Thread::ptr> threads;
  for (int i = 0; i < kThreads; ++i) {
    threads.push_back(lemo::thread::Thread::ptr(new lemo::thread::Thread(
        std::bind(mutex_worker, kOps), "mutex_" + std::to_string(i))));
  }
  for (size_t i = 0; i < threads.size(); ++i) {
    threads[i]->join();
  }
  LEMO_CHECK(g_counter == kThreads * kOps);
}

lemo::thread::RWMutex g_rwmutex;
int g_data = 0;

void reader_worker() {
  for (int i = 0; i < 100; ++i) {
    lemo::thread::RWMutex::ReadLock lock(g_rwmutex);
    (void)g_data;
  }
}

void writer_worker() {
  for (int i = 0; i < 10; ++i) {
    lemo::thread::RWMutex::WriteLock lock(g_rwmutex);
    ++g_data;
  }
}

void test_rwmutex() {
  g_data = 0;
  lemo::thread::Thread::ptr reader(
      new lemo::thread::Thread(reader_worker, "reader"));
  lemo::thread::Thread::ptr writer(
      new lemo::thread::Thread(writer_worker, "writer"));
  reader->join();
  writer->join();
  LEMO_CHECK(g_data == 10);
}

void test_tls() {
  std::string child_name;
  lemo::thread::Thread::ptr child(new lemo::thread::Thread(
      [&child_name]() {
        LEMO_CHECK(lemo::thread::Thread::GetThis() != nullptr);
        LEMO_CHECK(lemo::thread::Thread::GetThis()->getId() ==
                   lemo::utils::GetThreadId());
        child_name = lemo::thread::Thread::GetName();
      },
      "tls_child"));
  child->join();
  LEMO_CHECK(child_name == "tls_child");
}

void test_set_name() {
  lemo::thread::Thread::SetName("main_thread");
  LEMO_CHECK(lemo::utils::GetThreadName() == "main_thread");
}

}  // namespace

int main() {
  test_set_name();
  test_basic_thread();
  test_mutex();
  test_rwmutex();
  test_tls();
  std::printf("PASS test_thread\n");
  return 0;
}
