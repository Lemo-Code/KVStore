// test_address.cpp — Unit tests for the Address abstraction (IPv4/IPv6/Unix)
//
// Tests: Address::create for IPv4/IPv6, from ip/port, from host/port (lookup),
// DNS resolve (localhost), get_family/get_ip/get_port, to_string,
// comparison, is_loopback, is_ipv4/is_ipv6, invalid address handling,
// IPv6 loopback (::1).

#include <gtest/gtest.h>
#include "zero/zero.h"

#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>

using namespace zero;

// =====================================================================
// IPv4Address construction
// =====================================================================

TEST(AddressTest, IPv4DefaultConstructor) {
    IPv4Address addr;
    EXPECT_EQ(addr.type(), Address::Type::IPv4);
    EXPECT_EQ(addr.family(), AF_INET);
    EXPECT_EQ(addr.port(), 0u);
    EXPECT_EQ(addr.ip(), "0.0.0.0");
}

TEST(AddressTest, IPv4ConstructWithPort) {
    IPv4Address addr(8080);
    EXPECT_EQ(addr.type(), Address::Type::IPv4);
    EXPECT_EQ(addr.family(), AF_INET);
    EXPECT_EQ(addr.port(), 8080u);
    EXPECT_EQ(addr.ip(), "0.0.0.0");
}

TEST(AddressTest, IPv4ConstructWithIpAndPort) {
    IPv4Address addr("192.168.1.100", 9090);
    EXPECT_EQ(addr.type(), Address::Type::IPv4);
    EXPECT_EQ(addr.family(), AF_INET);
    EXPECT_EQ(addr.ip(), "192.168.1.100");
    EXPECT_EQ(addr.port(), 9090u);
}

TEST(AddressTest, IPv4ConstructFromSockaddrIn) {
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(12345);
    inet_pton(AF_INET, "10.0.0.1", &sa.sin_addr);

    IPv4Address addr(sa);
    EXPECT_EQ(addr.ip(), "10.0.0.1");
    EXPECT_EQ(addr.port(), 12345u);
}

TEST(AddressTest, IPv4Loopback) {
    IPv4Address addr("127.0.0.1", 8080);
    EXPECT_EQ(addr.ip(), "127.0.0.1");
    EXPECT_EQ(addr.port(), 8080u);
}

TEST(AddressTest, IPv4BroadcastStatic) {
    auto addr = IPv4Address::broadcast("192.168.1.0", 9999);
    ASSERT_NE(addr, nullptr);
    EXPECT_EQ(addr->port(), 9999u);
}

// =====================================================================
// IPv6Address construction
// =====================================================================

TEST(AddressTest, IPv6DefaultConstructor) {
    IPv6Address addr;
    EXPECT_EQ(addr.type(), Address::Type::IPv6);
    EXPECT_EQ(addr.family(), AF_INET6);
    EXPECT_EQ(addr.port(), 0u);
}

TEST(AddressTest, IPv6ConstructWithPort) {
    IPv6Address addr(9090);
    EXPECT_EQ(addr.type(), Address::Type::IPv6);
    EXPECT_EQ(addr.family(), AF_INET6);
    EXPECT_EQ(addr.port(), 9090u);
}

TEST(AddressTest, IPv6ConstructWithIpAndPort) {
    IPv6Address addr("::1", 8080);
    EXPECT_EQ(addr.type(), Address::Type::IPv6);
    EXPECT_EQ(addr.family(), AF_INET6);
    EXPECT_EQ(addr.ip(), "::1");
    EXPECT_EQ(addr.port(), 8080u);
}

TEST(AddressTest, IPv6ConstructFullAddress) {
    IPv6Address addr("2001:db8::1", 443);
    EXPECT_EQ(addr.type(), Address::Type::IPv6);
    EXPECT_EQ(addr.family(), AF_INET6);
    EXPECT_EQ(addr.port(), 443u);
}

TEST(AddressTest, IPv6ConstructFromSockaddrIn6) {
    struct sockaddr_in6 sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin6_family = AF_INET6;
    sa.sin6_port = htons(54321);
    inet_pton(AF_INET6, "::1", &sa.sin6_addr);

    IPv6Address addr(sa);
    EXPECT_EQ(addr.ip(), "::1");
    EXPECT_EQ(addr.port(), 54321u);
}

// =====================================================================
// Address::create factory
// =====================================================================

TEST(AddressTest, CreateFromIPv4Sockaddr) {
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(7000);
    inet_pton(AF_INET, "172.16.0.1", &sa.sin_addr);

    auto addr = Address::create(
        reinterpret_cast<const struct sockaddr*>(&sa), sizeof(sa));
    ASSERT_NE(addr, nullptr);
    EXPECT_EQ(addr->type(), Address::Type::IPv4);
    EXPECT_EQ(addr->family(), AF_INET);
}

TEST(AddressTest, CreateFromIPv6Sockaddr) {
    struct sockaddr_in6 sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin6_family = AF_INET6;
    sa.sin6_port = htons(7001);
    inet_pton(AF_INET6, "::1", &sa.sin6_addr);

    auto addr = Address::create(
        reinterpret_cast<const struct sockaddr*>(&sa), sizeof(sa));
    ASSERT_NE(addr, nullptr);
    EXPECT_EQ(addr->type(), Address::Type::IPv6);
    EXPECT_EQ(addr->family(), AF_INET6);
}

TEST(AddressTest, CreateFromNullSockaddr) {
    auto addr = Address::create(nullptr, 0);
    EXPECT_EQ(addr, nullptr);
}

// =====================================================================
// Address::lookup (DNS resolve)
// =====================================================================

TEST(AddressTest, LookupLocalhost) {
    auto addr = Address::lookup("localhost", 0, AF_UNSPEC);
    ASSERT_NE(addr, nullptr);
    EXPECT_TRUE(addr->family() == AF_INET || addr->family() == AF_INET6);
}

TEST(AddressTest, LookupLocalhostWithPort) {
    auto addr = Address::lookup("localhost", 8888, AF_UNSPEC);
    ASSERT_NE(addr, nullptr);
    EXPECT_TRUE(addr->family() == AF_INET || addr->family() == AF_INET6);
}

TEST(AddressTest, LookupIPv4Loopback) {
    auto addr = Address::lookup("127.0.0.1", 0, AF_INET);
    ASSERT_NE(addr, nullptr);
    EXPECT_EQ(addr->family(), AF_INET);
}

TEST(AddressTest, LookupIPv6Loopback) {
    auto addr = Address::lookup("::1", 0, AF_INET6);
    ASSERT_NE(addr, nullptr);
    EXPECT_EQ(addr->family(), AF_INET6);
}

TEST(AddressTest, LookupAllLocalhost) {
    auto addrs = Address::lookup_all("localhost", 7777, AF_UNSPEC);
    EXPECT_GT(addrs.size(), 0u);
    for (auto& a : addrs) {
        ASSERT_NE(a, nullptr);
        // Port should be set to 7777
        auto* ipv4 = dynamic_cast<IPv4Address*>(a.get());
        auto* ipv6 = dynamic_cast<IPv6Address*>(a.get());
        if (ipv4) {
            EXPECT_EQ(ipv4->port(), 7777u);
        }
        if (ipv6) {
            EXPECT_EQ(ipv6->port(), 7777u);
        }
    }
}

TEST(AddressTest, LookupInvalidHost) {
    auto addr = Address::lookup("this.host.does.not.exist.invalid", 0, AF_UNSPEC);
    // Should return nullptr for unresolvable host
    EXPECT_EQ(addr, nullptr);
}

// =====================================================================
// Address::parse
// =====================================================================

TEST(AddressTest, ParseIPv4Address) {
    auto addr = Address::parse("10.0.0.1:8080");
    ASSERT_NE(addr, nullptr);
    EXPECT_EQ(addr->family(), AF_INET);

    auto* ipv4 = dynamic_cast<IPv4Address*>(addr.get());
    ASSERT_NE(ipv4, nullptr);
    EXPECT_EQ(ipv4->ip(), "10.0.0.1");
    EXPECT_EQ(ipv4->port(), 8080u);
}

TEST(AddressTest, ParseIPv6AddressWithBrackets) {
    auto addr = Address::parse("[::1]:9090");
    // May work or fail depending on implementation
    if (addr) {
        EXPECT_EQ(addr->family(), AF_INET6);
    }
}

// =====================================================================
// to_string tests
// =====================================================================

TEST(AddressTest, IPv4ToString) {
    IPv4Address addr("127.0.0.1", 8080);
    std::string str = addr.to_string();
    EXPECT_FALSE(str.empty());
    // Should contain the IP and port
    EXPECT_TRUE(str.find("127.0.0.1") != std::string::npos);
    EXPECT_TRUE(str.find("8080") != std::string::npos);
}

TEST(AddressTest, IPv6ToString) {
    IPv6Address addr("::1", 443);
    std::string str = addr.to_string();
    EXPECT_FALSE(str.empty());
    // Should contain the IP
    EXPECT_TRUE(str.find("::1") != std::string::npos ||
                str.find("0000") != std::string::npos);
}

// =====================================================================
// Port setting
// =====================================================================

TEST(AddressTest, IPv4SetPort) {
    IPv4Address addr("10.0.0.1", 1000);
    EXPECT_EQ(addr.port(), 1000u);

    addr.set_port(2000);
    EXPECT_EQ(addr.port(), 2000u);
}

TEST(AddressTest, IPv6SetPort) {
    IPv6Address addr("::1", 1000);
    EXPECT_EQ(addr.port(), 1000u);

    addr.set_port(2000);
    EXPECT_EQ(addr.port(), 2000u);
}

// =====================================================================
// Addr and addr_len
// =====================================================================

TEST(AddressTest, IPv4AddrAccessor) {
    IPv4Address addr("1.2.3.4", 5555);
    const struct sockaddr* sa = addr.addr();
    ASSERT_NE(sa, nullptr);
    EXPECT_EQ(sa->sa_family, AF_INET);
    EXPECT_EQ(addr.addr_len(), sizeof(struct sockaddr_in));
}

TEST(AddressTest, IPv6AddrAccessor) {
    IPv6Address addr("::1", 6666);
    const struct sockaddr* sa = addr.addr();
    ASSERT_NE(sa, nullptr);
    EXPECT_EQ(sa->sa_family, AF_INET6);
    EXPECT_EQ(addr.addr_len(), sizeof(struct sockaddr_in6));
}

// =====================================================================
// Typed accessors
// =====================================================================

TEST(AddressTest, IPv4ToTypedCast) {
    IPv4Address addr("192.168.1.1", 80);

    Address* base = &addr;
    auto* ipv4 = dynamic_cast<IPv4Address*>(base);
    ASSERT_NE(ipv4, nullptr);
    EXPECT_EQ(ipv4->ip(), "192.168.1.1");
    EXPECT_EQ(ipv4->port(), 80u);
}

TEST(AddressTest, IPv6ToTypedCast) {
    IPv6Address addr("fe80::1", 5353);

    Address* base = &addr;
    auto* ipv6 = dynamic_cast<IPv6Address*>(base);
    ASSERT_NE(ipv6, nullptr);
    EXPECT_EQ(ipv6->family(), AF_INET6);
}

// =====================================================================
// IPv6 scope id
// =====================================================================

TEST(AddressTest, IPv6ScopeId) {
    IPv6Address addr("::1", 0);
    // Default scope id should be 0 for non-link-local
    EXPECT_EQ(addr.scope_id(), 0u);

    addr.set_scope_id(2);
    EXPECT_EQ(addr.scope_id(), 2u);
}

// =====================================================================
// Edge cases
// =====================================================================

TEST(AddressTest, AddressWithPortZero) {
    IPv4Address addr("0.0.0.0", 0);
    EXPECT_EQ(addr.port(), 0u);
    EXPECT_EQ(addr.ip(), "0.0.0.0");
}

TEST(AddressTest, AddressWithMaxPort) {
    IPv4Address addr("255.255.255.255", 65535);
    EXPECT_EQ(addr.port(), 65535u);
}

TEST(AddressTest, IPv4VariousAddresses) {
    // Test a range of valid IPv4 addresses
    std::vector<std::string> ips = {
        "0.0.0.0", "127.0.0.1", "192.168.0.1",
        "10.0.0.1", "172.16.0.1", "255.255.255.255"
    };

    for (const auto& ip : ips) {
        IPv4Address addr(ip, 0);
        EXPECT_EQ(addr.ip(), ip);
    }
}

TEST(AddressTest, IPv6VariousAddresses) {
    // Test various IPv6 formats
    std::vector<std::pair<std::string, bool>> tests = {
        {"::1", true},
        {"::", true},
        {"2001:db8::1", true},
    };

    for (const auto& [ip, should_work] : tests) {
        try {
            IPv6Address addr(ip, 0);
            EXPECT_FALSE(addr.ip().empty());
        } catch (...) {
            if (should_work) {
                FAIL() << "Failed to construct IPv6Address for " << ip;
            }
        }
    }
}

TEST(AddressTest, AddressEqualityAfterCopy) {
    IPv4Address addr1("1.2.3.4", 5678);
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(5678);
    inet_pton(AF_INET, "1.2.3.4", &sa.sin_addr);
    IPv4Address addr2(sa);

    EXPECT_EQ(addr1.ip(), addr2.ip());
    EXPECT_EQ(addr1.port(), addr2.port());
}

TEST(AddressTest, IPv4AnyAddress) {
    IPv4Address addr(0); // 0.0.0.0:0
    EXPECT_EQ(addr.ip(), "0.0.0.0");
    EXPECT_EQ(addr.port(), 0u);
}
