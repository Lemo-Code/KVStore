/**
 * @file test_address.cc
 * @brief Address / IPv4Address 基础解析
 */
#include "../io/test_common.h"

#include "lemo/socket/address.h"

#include <cstdio>

namespace {

void test_ipv4_create() {
  lemo::socket::IPv4Address::ptr addr =
      lemo::socket::IPv4Address::Create("127.0.0.1", 8080);
  LEMO_CHECK(addr != nullptr);
  LEMO_CHECK(addr->getPort() == 8080);
  const std::string s = addr->toString();
  LEMO_CHECK(s.find("127.0.0.1") != std::string::npos);
  LEMO_CHECK(s.find("8080") != std::string::npos);
}

void test_lookup_any() {
  lemo::socket::Address::ptr addr =
      lemo::socket::Address::LookupAny("localhost");
  LEMO_CHECK(addr != nullptr);
  LEMO_CHECK(addr->getFamily() == AF_INET || addr->getFamily() == AF_INET6);
}

}  // namespace

int main() {
  test_ipv4_create();
  test_lookup_any();
  std::printf("PASS test_address\n");
  return 0;
}
