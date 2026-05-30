/**
 * @file test_thread.cc
 * @brief net 线程模块：Thread / Mutex / RWMutex / TLS
 */
#include "test_common.h"
#include "thread/module.h"
#include "common/util.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace {

void test_basic_thread() {
  std::atomic<int> done{0};
  std::vector<net::Thread::ptr> threads;
  for (int i = 0; i < 3; ++i) {
    threads.push_back(net::Thread::ptr(new net::Thread(
        [&done, i]() {
          NET_CHECK(net::Thread::GetThis() != nullptr);
          NET_CHECK(net::Thread::GetThis()->getId() == net::GetThreadId());
          NET_CHECK(!net::Thread::GetName().empty());
          ++done;
        },
        "worker_" + std::to_string(i))));
  }
  for (auto& t : threads) {
    t->join();
  }
  NET_CHECK(done.load() == 3);
}

net::Mutex g_mutex;
int g_counter = 0;

void mutex_worker(int ops) {
  for (int i = 0; i < ops; ++i) {
    net::Mutex::Lock lock(g_mutex);
    ++g_counter;
  }
}

void test_mutex() {
  g_counter = 0;
  const int kThreads = 4;
  const int kOps = 10000;
  std::vector<net::Thread::ptr> threads;
  for (int i = 0; i < kThreads; ++i) {
    threads.push_back(net::Thread::ptr(
        new net::Thread(std::bind(mutex_worker, kOps), "mutex_" + std::to_string(i))));
  }
  for (auto& t : threads) {
    t->join();
  }
  NET_CHECK(g_counter == kThreads * kOps);
}

net::RWMutex g_rwmutex;
int g_data = 0;

void reader_worker() {
  for (int i = 0; i < 100; ++i) {
    net::RWMutex::ReadLock lock(g_rwmutex);
    (void)g_data;
    std::this_thread::sleep_for(std::chrono::microseconds(10));
  }
}

void writer_worker() {
  for (int i = 0; i < 10; ++i) {
    net::RWMutex::WriteLock lock(g_rwmutex);
    ++g_data;
    std::this_thread::sleep_for(std::chrono::microseconds(50));
  }
}

void test_rwmutex() {
  g_data = 0;
  const int kReaders = 5;
  const int kWriters = 2;
  std::vector<net::Thread::ptr> threads;
  for (int i = 0; i < kReaders; ++i) {
    threads.push_back(
        net::Thread::ptr(new net::Thread(reader_worker, "reader_" + std::to_string(i))));
  }
  for (int i = 0; i < kWriters; ++i) {
    threads.push_back(
        net::Thread::ptr(new net::Thread(writer_worker, "writer_" + std::to_string(i))));
  }
  for (auto& t : threads) {
    t->join();
  }
  NET_CHECK(g_data == kWriters * 10);
}

thread_local int g_tls = 0;

void tls_worker(int id) {
  g_tls = id * 100;
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  NET_CHECK(g_tls == id * 100);
}

void test_thread_local_storage() {
  g_tls = -1;
  std::vector<net::Thread::ptr> threads;
  for (int i = 0; i < 3; ++i) {
    threads.push_back(net::Thread::ptr(
        new net::Thread(std::bind(tls_worker, i), "tls_" + std::to_string(i))));
  }
  for (auto& t : threads) {
    t->join();
  }
  NET_CHECK(g_tls == -1);
}

void test_set_name() {
  net::Thread::SetName("main_test");
  NET_CHECK(net::GetThreadName() == "main_test");
}

}  // namespace

int main() {
  test_set_name();
  test_basic_thread();
  test_mutex();
  test_rwmutex();
  test_thread_local_storage();
  std::printf("test_thread: OK\n");
  return 0;
}
