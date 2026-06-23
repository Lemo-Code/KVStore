// zero Address implementation
//
// Provides concrete implementations of the Address abstraction:
//   IPv4Address — standard dotted-quad IPv4 addresses
//   IPv6Address — colon-separated IPv6 addresses
//   UnixAddress — local filesystem socket paths
//
// DNS resolution is performed via getaddrinfo(3). All string
// conversions use inet_ntop/inet_pton for accuracy.
#include "zero/net/address.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <cstring>
#include <sstream>

namespace zero {

// ============================================================
// Address factory
// ============================================================
std::shared_ptr<Address> Address::create(const struct sockaddr* addr,
                                          socklen_t /*len*/) {
    if (!addr) return nullptr;

    switch (addr->sa_family) {
    case AF_INET: {
        const auto* in4 = reinterpret_cast<const struct sockaddr_in*>(addr);
        return std::make_shared<IPv4Address>(*in4);
    }
    case AF_INET6: {
        const auto* in6 = reinterpret_cast<const struct sockaddr_in6*>(addr);
        return std::make_shared<IPv6Address>(*in6);
    }
    case AF_UNIX: {
        const auto* un = reinterpret_cast<const struct sockaddr_un*>(addr);
        return std::make_shared<UnixAddress>(un->sun_path);
    }
    default:
        return nullptr;
    }
}

std::shared_ptr<Address> Address::lookup(const std::string& host, int port, int family) {
    if (host.empty()) return nullptr;

    struct addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = family;      // Support both IPv4 and IPv6
    hints.ai_socktype = SOCK_STREAM;  // TCP by default

    std::string port_str = std::to_string(port);
    struct addrinfo* result = nullptr;

    int ret = ::getaddrinfo(host.c_str(),
                            port > 0 ? port_str.c_str() : nullptr,
                            &hints, &result);
    if (ret != 0 || !result) {
        return nullptr;
    }

    // Return the first resolved address
    std::shared_ptr<Address> addr = create(result->ai_addr, result->ai_addrlen);
    ::freeaddrinfo(result);
    return addr;
}

// ============================================================
// IPv4Address
// ============================================================
IPv4Address::IPv4Address(const struct sockaddr_in& addr)
    : addr_(addr) {}

IPv4Address::IPv4Address(const std::string& ip, uint16_t port) {
    std::memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = ::htons(port);

    if (ip.empty() || ip == "0.0.0.0") {
        addr_.sin_addr.s_addr = INADDR_ANY;
    } else {
        ::inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr);
    }
}

IPv4Address::IPv4Address(uint16_t port) {
    std::memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = ::htons(port);
    addr_.sin_addr.s_addr = INADDR_ANY;
}


std::string IPv4Address::ip() const {
    char buf[INET_ADDRSTRLEN] = {};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    return std::string(buf);
}

uint16_t IPv4Address::port() const {
    return ::ntohs(addr_.sin_port);
}


// ============================================================
// IPv6Address
// ============================================================
IPv6Address::IPv6Address(const struct sockaddr_in6& addr)
    : addr_(addr) {}

IPv6Address::IPv6Address(const std::string& ip, uint16_t port) {
    std::memset(&addr_, 0, sizeof(addr_));
    addr_.sin6_family = AF_INET6;
    addr_.sin6_port = ::htons(port);

    if (ip.empty() || ip == "::") {
        addr_.sin6_addr = in6addr_any;
    } else {
        ::inet_pton(AF_INET6, ip.c_str(), &addr_.sin6_addr);
    }
}

IPv6Address::IPv6Address(uint16_t port) {
    std::memset(&addr_, 0, sizeof(addr_));
    addr_.sin6_family = AF_INET6;
    addr_.sin6_port = ::htons(port);
    addr_.sin6_addr = in6addr_any;
}


std::string IPv6Address::ip() const {
    char buf[INET6_ADDRSTRLEN] = {};
    ::inet_ntop(AF_INET6, &addr_.sin6_addr, buf, sizeof(buf));
    return std::string(buf);
}

uint16_t IPv6Address::port() const {
    return ::ntohs(addr_.sin6_port);
}

// ============================================================
// UnixAddress
// ============================================================
UnixAddress::UnixAddress(const std::string& path) {
    std::memset(&addr_, 0, sizeof(addr_));
    addr_.sun_family = AF_UNIX;
    std::strncpy(addr_.sun_path, path.c_str(), sizeof(addr_.sun_path) - 1);
}


} // namespace zero
