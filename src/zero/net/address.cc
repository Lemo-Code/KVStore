#include "zero/net/address.h"

#include <cstring>
#include <sstream>
#include <ifaddrs.h>
#include <netdb.h>

namespace zero {

// ====================================================================
// Address base
// ====================================================================
Address::ptr Address::Create(const sockaddr* addr, socklen_t len) {
    if (!addr) return nullptr;

    switch (addr->sa_family) {
        case AF_INET:
            return std::make_shared<IPv4Address>(*reinterpret_cast<const sockaddr_in*>(addr));
        case AF_INET6:
            return std::make_shared<IPv6Address>(*reinterpret_cast<const sockaddr_in6*>(addr));
        case AF_UNIX:
            return std::make_shared<UnixAddress>();  // simplified
        default:
            return nullptr;
    }
}

bool Address::Lookup(std::vector<Address::ptr>& result, const std::string& host,
                     int family, int type, int protocol) {
    struct addrinfo hints, *res, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = family;
    hints.ai_socktype = type;
    hints.ai_protocol = protocol;

    int ret = getaddrinfo(host.c_str(), nullptr, &hints, &res);
    if (ret != 0) return false;

    for (rp = res; rp; rp = rp->ai_next) {
        auto addr = Create(rp->ai_addr, rp->ai_addrlen);
        if (addr) result.push_back(addr);
    }

    freeaddrinfo(res);
    return !result.empty();
}

Address::ptr Address::LookupAny(const std::string& host, int family, int type, int protocol) {
    std::vector<Address::ptr> result;
    if (Lookup(result, host, family, type, protocol)) {
        return result[0];
    }
    return nullptr;
}

std::string Address::toString() const {
    std::stringstream ss;
    insert(ss);
    return ss.str();
}

bool Address::operator<(const Address& rhs) const {
    socklen_t len = std::min(getAddrLen(), rhs.getAddrLen());
    int cmp = memcmp(getAddr(), rhs.getAddr(), len);
    if (cmp < 0) return true;
    if (cmp > 0) return false;
    return getAddrLen() < rhs.getAddrLen();
}

bool Address::operator==(const Address& rhs) const {
    return getAddrLen() == rhs.getAddrLen()
        && memcmp(getAddr(), rhs.getAddr(), getAddrLen()) == 0;
}

bool Address::operator!=(const Address& rhs) const { return !(*this == rhs); }

bool Address::GetInterfaceAddresses(
    std::multimap<std::string, std::pair<Address::ptr, uint32_t>>& result,
    int family) {
    struct ifaddrs* ifaddr;
    if (getifaddrs(&ifaddr) == -1) return false;

    for (struct ifaddrs* ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (family != AF_UNSPEC && ifa->ifa_addr->sa_family != family) continue;

        auto addr = Create(ifa->ifa_addr, sizeof(sockaddr));
        if (addr) {
            uint32_t prefix_len = 0;
            if (ifa->ifa_netmask) {
                // Count bits in netmask
                auto* mask = reinterpret_cast<sockaddr_in*>(ifa->ifa_netmask);
                prefix_len = __builtin_popcount(mask->sin_addr.s_addr);
            }
            result.emplace(ifa->ifa_name, std::make_pair(addr, prefix_len));
        }
    }
    freeifaddrs(ifaddr);
    return !result.empty();
}

// ====================================================================
// IPv4
// ====================================================================
IPv4Address::IPv4Address(uint32_t addr, uint16_t port) {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_addr.s_addr = htonl(addr);
    addr_.sin_port = htons(port);
}

IPv4Address::IPv4Address(const sockaddr_in& addr) : addr_(addr) {}

IPv4Address::ptr IPv4Address::Create(const char* addr, uint16_t port) {
    auto result = std::make_shared<IPv4Address>();
    result->addr_.sin_port = htons(port);
    if (addr && inet_pton(AF_INET, addr, &result->addr_.sin_addr) <= 0) {
        return nullptr;
    }
    return result;
}

std::ostream& IPv4Address::insert(std::ostream& os) const {
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    os << buf << ":" << getPort();
    return os;
}

std::string IPv4Address::getIPString() const {
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    return buf;
}

IPv4Address::ptr IPv4Address::broadcastAddress(uint32_t prefix_len) const {
    uint32_t ip = ntohl(addr_.sin_addr.s_addr);
    uint32_t mask = (prefix_len >= 32) ? 0 : ~((1u << (32 - prefix_len)) - 1);
    return std::make_shared<IPv4Address>(ip | ~mask, getPort());
}

IPv4Address::ptr IPv4Address::networkAddress(uint32_t prefix_len) const {
    uint32_t ip = ntohl(addr_.sin_addr.s_addr);
    uint32_t mask = (prefix_len >= 32) ? 0 : ~((1u << (32 - prefix_len)) - 1);
    return std::make_shared<IPv4Address>(ip & mask, getPort());
}

IPv4Address::ptr IPv4Address::subnetMask(uint32_t prefix_len) const {
    uint32_t mask = (prefix_len >= 32) ? 0 : ~((1u << (32 - prefix_len)) - 1);
    return std::make_shared<IPv4Address>(mask, 0);
}

// ====================================================================
// IPv6
// ====================================================================
IPv6Address::IPv6Address() {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sin6_family = AF_INET6;
}

IPv6Address::IPv6Address(const sockaddr_in6& addr) : addr_(addr) {}

IPv6Address::IPv6Address(const uint8_t addr[16], uint16_t port) {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sin6_family = AF_INET6;
    memcpy(&addr_.sin6_addr, addr, 16);
    addr_.sin6_port = htons(port);
}

IPv6Address::ptr IPv6Address::Create(const char* addr, uint16_t port) {
    auto result = std::make_shared<IPv6Address>();
    result->addr_.sin6_port = htons(port);
    if (addr && inet_pton(AF_INET6, addr, &result->addr_.sin6_addr) <= 0) {
        return nullptr;
    }
    return result;
}

std::ostream& IPv6Address::insert(std::ostream& os) const {
    char buf[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &addr_.sin6_addr, buf, sizeof(buf));
    os << "[" << buf << "]:" << getPort();
    return os;
}

std::string IPv6Address::getIPString() const {
    char buf[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &addr_.sin6_addr, buf, sizeof(buf));
    return buf;
}

// ====================================================================
// Unix
// ====================================================================
UnixAddress::UnixAddress() {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sun_family = AF_UNIX;
    addr_len_ = sizeof(sa_family_t);
}

UnixAddress::UnixAddress(const std::string& path) : UnixAddress() {
    size_t len = std::min(path.size(), sizeof(addr_.sun_path) - 1);
    memcpy(addr_.sun_path, path.c_str(), len);
    addr_.sun_path[len] = '\0';
    addr_len_ = offsetof(sockaddr_un, sun_path) + len + 1;
}

std::ostream& UnixAddress::insert(std::ostream& os) const {
    os << "unix:" << addr_.sun_path;
    return os;
}

// ====================================================================
// operator<<
// ====================================================================
std::ostream& operator<<(std::ostream& os, const Address& addr) {
    return addr.insert(os);
}

} // namespace zero
