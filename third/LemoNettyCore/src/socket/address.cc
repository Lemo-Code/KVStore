#include "lemo/socket/address.h"

#include "lemo/utils/endian.h"

#include <algorithm>
#include <ifaddrs.h>
#include <iostream>
#include <netdb.h>
#include <stdexcept>
#include <string.h>

namespace {

template <class T>
T CreateMask(uint32_t bits) {
  return (1 << (sizeof(T) * 8 - bits)) - 1;
}

template <class T>
uint32_t CountBytes(T value) {
  uint32_t result = 0;
  for (; value; result++) {
    value &= value - 1;
  }
  return result;
}

}  // namespace

namespace lemo::socket {

Address::ptr Address::Create(const sockaddr* addr, socklen_t addr_len) {
  if (addr == nullptr) {
    return nullptr;
  }

  Address::ptr result;
  switch (addr->sa_family) {
    case AF_INET:
      result.reset(new IPv4Address(*(const sockaddr_in*)addr));
      break;
    case AF_INET6:
      result.reset(new IPv6Address(*(const sockaddr_in6*)addr));
      break;
    default:
      result.reset(new UnknownAddress(*addr));
      break;
  }
  return result;
}

bool Address::Lookup(std::vector<Address::ptr>& result, const std::string& host,
                     int family, int type, int protocol) {
  addrinfo hints, *results, *next;
  hints.ai_flags = 0;
  hints.ai_family = family;
  hints.ai_socktype = type;
  hints.ai_protocol = protocol;
  hints.ai_addrlen = 0;
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;

  std::string node;
  const char* service = NULL;

  if (!host.empty() && host[0] == '[') {
    const char* endipv6 =
        (const char*)memchr(host.c_str() + 1, ']', host.size() - 1);
    if (endipv6) {
      if (*(endipv6 + 1) == ':') {
        service = endipv6 + 2;
      }
      node = host.substr(1, endipv6 - host.c_str() - 1);
    }
  }

  if (node.empty()) {
    service = (const char*)memchr(host.c_str(), ':', host.size());
    if (service) {
      if (!memchr(service + 1, ':', host.c_str() + host.size() - service - 1)) {
        node = host.substr(0, service - host.c_str());
        ++service;
      }
    }
  }

  if (node.empty()) {
    node = host;
  }
  int error = getaddrinfo(node.c_str(), service, &hints, &results);
  if (error) {
    return false;
  }

  next = results;
  while (next) {
    result.push_back(Address::Create(next->ai_addr, (socklen_t)next->ai_addrlen));
    next = next->ai_next;
  }

  freeaddrinfo(results);
  return !result.empty();
}

Address::ptr Address::LookupAny(const std::string& host, int family, int type,
                                int protocol) {
  std::vector<Address::ptr> result;
  if (Lookup(result, host, family, type, protocol)) {
    return result[0];
  }
  return nullptr;
}

IPAddress::ptr Address::LookupAnyIPAddress(const std::string& host, int family,
                                           int type, int protocol) {
  std::vector<Address::ptr> result;
  if (Lookup(result, host, family, type, protocol)) {
    for (auto& i : result) {
      IPAddress::ptr v = std::dynamic_pointer_cast<IPAddress>(i);
      if (v) {
        return v;
      }
    }
  }
  return nullptr;
}

std::shared_ptr<UnixAddress> Address::LookupAnyUnixAddress(
    const std::string& host, int family, int type, int protocol) {
  std::vector<Address::ptr> result;
  if (Lookup(result, host, family, type, protocol)) {
    for (auto& i : result) {
      UnixAddress::ptr v = std::dynamic_pointer_cast<UnixAddress>(i);
      if (v) {
        return v;
      }
    }
  }
  return nullptr;
}

int Address::getFamily() const { return getAddr()->sa_family; }

std::string Address::toString() const {
  std::stringstream ss;
  insert(ss);
  return ss.str();
}

bool Address::operator<(const Address& rhs) const {
  socklen_t minlen = std::min(getAddrLen(), rhs.getAddrLen());
  int res = memcmp(getAddr(), rhs.getAddr(), minlen);
  if (res < 0) {
    return true;
  } else if (res > 0) {
    return false;
  } else if (getAddrLen() < rhs.getAddrLen()) {
    return true;
  }
  return false;
}

bool Address::operator==(const Address& rhs) const {
  return getAddrLen() == rhs.getAddrLen() &&
         memcmp(getAddr(), rhs.getAddr(), getAddrLen()) == 0;
}

bool Address::operator!=(const Address& rhs) const { return !(*this == rhs); }

IPv4Address::ptr IPv4Address::Create(const char* address, uint16_t port) {
  IPv4Address::ptr rt(new IPv4Address);
  rt->addr_.sin_port = lemo::utils::byteswapOnLittleEndian(port);
  rt->addr_.sin_family = AF_INET;
  int res = inet_pton(AF_INET, address, &rt->addr_.sin_addr.s_addr);
  if (res <= 0) {
    return nullptr;
  }
  return rt;
}

IPv4Address::IPv4Address(uint32_t address, uint16_t port) {
  memset(&addr_, 0, sizeof(addr_));
  addr_.sin_family = AF_INET;
  addr_.sin_port = lemo::utils::byteswapOnLittleEndian(port);
  addr_.sin_addr.s_addr = lemo::utils::byteswapOnLittleEndian(address);
}

IPv4Address::IPv4Address(const sockaddr_in& addr) { addr_ = addr; }

std::ostream& IPv4Address::insert(std::ostream& os) const {
  uint32_t addr = lemo::utils::byteswapOnLittleEndian(addr_.sin_addr.s_addr);
  os << ((addr >> 24) & 0xff) << "." << ((addr >> 16) & 0xff) << "."
     << ((addr >> 8) & 0xff) << "." << (addr & 0xff);
  os << ":" << lemo::utils::byteswapOnLittleEndian(addr_.sin_port);
  return os;
}

std::ostream& IPv4Address::insertAddr(std::ostream& os) const {
  uint32_t addr = lemo::utils::byteswapOnLittleEndian(addr_.sin_addr.s_addr);
  os << ((addr >> 24) & 0xff) << "." << ((addr >> 16) & 0xff) << "."
     << ((addr >> 8) & 0xff) << "." << (addr & 0xff);
  return os;
}

std::string IPv4Address::AddrString() const {
  std::stringstream os;
  insertAddr(os);
  return os.str();
}

const sockaddr* IPv4Address::getAddr() const { return (sockaddr*)&addr_; }

sockaddr* IPv4Address::getAddr() { return (sockaddr*)&addr_; }

socklen_t IPv4Address::getAddrLen() const { return sizeof(addr_); }

IPAddress::ptr IPAddress::Create(const char* address, uint16_t port) {
  addrinfo hints, *results;
  memset(&hints, 0, sizeof(hints));

  hints.ai_flags = AI_NUMERICHOST;
  hints.ai_family = AF_UNSPEC;

  int error = getaddrinfo(address, NULL, &hints, &results);
  if (error) {
    return nullptr;
  }

  try {
    IPAddress::ptr result = std::dynamic_pointer_cast<IPAddress>(
        Address::Create(results->ai_addr, (socklen_t)results->ai_addrlen));
    if (result) {
      result->setPort(port);
    }
    freeaddrinfo(results);
    return result;
  } catch (...) {
    freeaddrinfo(results);
    return nullptr;
  }
}

IPAddress::ptr IPv4Address::broadcastAddress(uint32_t prefix_len) {
  if (prefix_len > 32) {
    return nullptr;
  }

  sockaddr_in baddr(addr_);
  baddr.sin_addr.s_addr |=
      lemo::utils::byteswapOnLittleEndian(CreateMask<uint32_t>(prefix_len));
  return IPv4Address::ptr(new IPv4Address(baddr));
}

IPAddress::ptr IPv4Address::networkAddress(uint32_t prefix_len) {
  if (prefix_len > 32) {
    return nullptr;
  }

  sockaddr_in baddr(addr_);
  baddr.sin_addr.s_addr &=
      ~lemo::utils::byteswapOnLittleEndian(CreateMask<uint32_t>(prefix_len));
  return IPv4Address::ptr(new IPv4Address(baddr));
}

IPAddress::ptr IPv4Address::subnetMask(uint32_t prefix_len) {
  sockaddr_in baddr;
  memset(&baddr, 0, sizeof(baddr));
  baddr.sin_family = AF_INET;
  baddr.sin_addr.s_addr =
      ~lemo::utils::byteswapOnLittleEndian(CreateMask<uint32_t>(prefix_len));
  return IPv4Address::ptr(new IPv4Address(baddr));
}

void IPv4Address::setPort(uint16_t v) {
  addr_.sin_port = lemo::utils::byteswapOnLittleEndian(v);
}

uint16_t IPv4Address::getPort() const {
  return lemo::utils::byteswapOnLittleEndian(addr_.sin_port);
}

IPv6Address::IPv6Address() {
  memset(&addr_, 0, sizeof(addr_));
  addr_.sin6_family = AF_INET6;
}

IPv6Address::ptr IPv6Address::Create(const char* address, uint16_t port) {
  std::shared_ptr<IPv6Address> rt(new IPv6Address);
  rt->addr_.sin6_port = lemo::utils::byteswapOnLittleEndian(port);
  int res = inet_pton(AF_INET6, address, &rt->addr_.sin6_addr.s6_addr);
  if (res != 1) {
    return nullptr;
  }
  return rt;
}

IPv6Address::IPv6Address(const uint8_t address[16], uint16_t port) {
  memset(&addr_, 0, sizeof(addr_));
  addr_.sin6_family = AF_INET6;
  addr_.sin6_port = lemo::utils::byteswapOnLittleEndian(port);
  memcpy(&addr_.sin6_addr.s6_addr, address, 16);
}

IPv6Address::IPv6Address(const sockaddr_in6& addr) { addr_ = addr; }

std::ostream& IPv6Address::insert(std::ostream& os) const {
  os << "[";
  uint16_t* addr = (uint16_t*)addr_.sin6_addr.s6_addr;
  bool used_zero = false;
  for (size_t i = 0; i < 8; i++) {
    if (addr[i] == 0 && !used_zero) {
      continue;
    }
    if (i && addr[i - 1] == 0 && !used_zero) {
      os << ":";
      used_zero = true;
    }
    if (i) {
      os << ":";
    }
    os << std::hex << (int)lemo::utils::byteswapOnLittleEndian(addr[i]) << std::dec;
  }

  if (!used_zero && addr[7] == 0) {
    os << "::";
  }

  os << "]:" << lemo::utils::byteswapOnLittleEndian(addr_.sin6_port);
  return os;
}

std::string IPv6Address::AddrString() const {
  std::stringstream os;
  insertAddr(os);
  return os.str();
}

std::ostream& IPv6Address::insertAddr(std::ostream& os) const {
  os << "[";
  uint16_t* addr = (uint16_t*)addr_.sin6_addr.s6_addr;
  bool used_zero = false;
  for (size_t i = 0; i < 8; i++) {
    if (addr[i] == 0 && !used_zero) {
      continue;
    }
    if (i && addr[i - 1] == 0 && !used_zero) {
      os << ":";
      used_zero = true;
    }
    if (i) {
      os << ":";
    }
    os << std::hex << (int)lemo::utils::byteswapOnLittleEndian(addr[i]) << std::dec;
  }

  if (!used_zero && addr[7] == 0) {
    os << "::";
  }

  os << "]";
  return os;
}

const sockaddr* IPv6Address::getAddr() const { return (sockaddr*)&addr_; }

sockaddr* IPv6Address::getAddr() { return (sockaddr*)&addr_; }

socklen_t IPv6Address::getAddrLen() const { return sizeof(addr_); }

IPAddress::ptr IPv6Address::broadcastAddress(uint32_t prefix_len) {
  sockaddr_in6 baddr(addr_);
  baddr.sin6_addr.s6_addr[prefix_len / 8] |= CreateMask<uint8_t>(prefix_len % 8);
  for (size_t i = prefix_len / 8 + 1; i < 16; i++) {
    baddr.sin6_addr.s6_addr[i] |= 0xff;
  }
  return IPv6Address::ptr(new IPv6Address(baddr));
}

IPAddress::ptr IPv6Address::networkAddress(uint32_t prefix_len) {
  sockaddr_in6 baddr(addr_);
  baddr.sin6_addr.s6_addr[prefix_len / 8] &=
      ~CreateMask<uint8_t>(prefix_len % 8);
  for (size_t i = prefix_len / 8 + 1; i < 16; i++) {
    baddr.sin6_addr.s6_addr[i] |= 0x00;
  }
  return IPv6Address::ptr(new IPv6Address(baddr));
}

IPAddress::ptr IPv6Address::subnetMask(uint32_t prefix_len) {
  sockaddr_in6 baddr;
  memset(&baddr, 0, sizeof(baddr));
  baddr.sin6_family = AF_INET6;
  for (size_t i = 0; i < prefix_len / 8; i++) {
    baddr.sin6_addr.s6_addr[i] = 0xFF;
  }
  baddr.sin6_addr.s6_addr[prefix_len / 8] =
      ~CreateMask<uint8_t>(prefix_len % 8);
  return IPv6Address::ptr(new IPv6Address(baddr));
}

void IPv6Address::setPort(uint16_t v) {
  addr_.sin6_port = lemo::utils::byteswapOnLittleEndian(v);
}

uint16_t IPv6Address::getPort() const {
  return lemo::utils::byteswapOnLittleEndian(addr_.sin6_port);
}

static const size_t MAX_PATH_LEN = sizeof(((sockaddr_un*)0)->sun_path) - 1;

UnixAddress::UnixAddress() {
  memset(&addr_, 0, sizeof(addr_));
  addr_.sun_family = AF_UNIX;
  length_ = offsetof(sockaddr_un, sun_path) + MAX_PATH_LEN;
}

UnixAddress::UnixAddress(const std::string& path) {
  memset(&addr_, 0, sizeof(addr_));
  addr_.sun_family = AF_UNIX;
  length_ = path.size() + 1;

  if (!path.empty() && path[0] == '\0') {
    --length_;
  }

  if (length_ > sizeof(addr_.sun_path)) {
    throw std::logic_error("unixAddr path too long");
  }
  memcpy(&addr_.sun_path, path.c_str(), length_);
  length_ += offsetof(sockaddr_un, sun_path);
}

const sockaddr* UnixAddress::getAddr() const { return (sockaddr*)&addr_; }

sockaddr* UnixAddress::getAddr() { return (sockaddr*)&addr_; }

socklen_t UnixAddress::getAddrLen() const { return length_; }

std::ostream& UnixAddress::insert(std::ostream& os) const {
  if (length_ > offsetof(sockaddr_un, sun_path) && addr_.sun_path[0] == '\0') {
    return os << "\\0" << std::string(addr_.sun_path + 1,
                                      length_ - offsetof(sockaddr_un, sun_path) -
                                          1);
  }
  return os << addr_.sun_path;
}

void UnixAddress::setAddrLen(uint32_t v) { length_ = v; }

UnknownAddress::UnknownAddress(int family) {
  memset(&addr_, 0, sizeof(addr_));
  addr_.sa_family = family;
}

UnknownAddress::UnknownAddress(const sockaddr& addr) { addr_ = addr; }

const sockaddr* UnknownAddress::getAddr() const { return &addr_; }

sockaddr* UnknownAddress::getAddr() { return &addr_; }

socklen_t UnknownAddress::getAddrLen() const { return sizeof(addr_); }

std::ostream& UnknownAddress::insert(std::ostream& os) const {
  os << "UnknownAddress family=" << addr_.sa_family << "]";
  return os;
}

std::ostream& operator<<(std::ostream& os, const Address& addr) {
  addr.insert(os);
  return os;
}

}  // namespace lemo::socket
