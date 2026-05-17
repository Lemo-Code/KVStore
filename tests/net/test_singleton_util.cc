#include "test_common.h"

#include "singleton.h"
#include "util.h"

#include <string>

namespace {

struct Counter {
  Counter() { ++constructed; }
  int value = 0;
  static int constructed;
};

int Counter::constructed = 0;

}  // namespace

int main() {
  NET_CHECK(net::GetThreadId() > 0);
  NET_CHECK(net::GetFiberId() == 0);
  NET_CHECK(!net::GetThreadName().empty());
  NET_CHECK(net::GetElapseMs() >= 0);
  NET_CHECK(net::GetCurrentMS() > 0);
  NET_CHECK(net::GetCurrentUS() > net::GetCurrentMS());
  NET_CHECK(!net::Time2Str(time(nullptr)).empty());

  auto* a = net::Singleton<Counter>::GetInstance();
  auto* b = net::Singleton<Counter>::GetInstance();
  NET_CHECK(a == b);
  NET_CHECK(Counter::constructed == 1);

  auto p1 = net::SingletonPtr<Counter>::GetInstance();
  auto p2 = net::SingletonPtr<Counter>::GetInstance();
  NET_CHECK(p1.get() == p2.get());

  std::printf("PASS test_singleton_util\n");
  return 0;
}
