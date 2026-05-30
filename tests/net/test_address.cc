/**
 * @file test_address.cc
 * @brief Address / IPv4Address 基础解析
 */
#include "test_common.h"

#include "socket/address.h"

#include <cstdio>

namespace {

void test_ipv4_create() {
  net::IPv4Address::ptr addr = net::IPv4Address::Create("127.0.0.1", 8080);
  NET_CHECK(addr != nullptr);
  NET_CHECK(addr->getPort() == 8080);
  const std::string s = addr->toString();
  NET_CHECK(s.find("127.0.0.1") != std::string::npos);
  NET_CHECK(s.find("8080") != std::string::npos);
}

void test_lookup_any() {
  net::Address::ptr addr = net::Address::LookupAny("localhost");
  NET_CHECK(addr != nullptr);
  NET_CHECK(addr->getFamily() == AF_INET || addr->getFamily() == AF_INET6);
}

}  // namespace

int main() {
  test_ipv4_create();
  test_lookup_any();
  std::printf("PASS test_address\n");
  return 0;
}
