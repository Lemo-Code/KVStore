#ifndef NET_SOCKET_ADDRESS_H
#define NET_SOCKET_ADDRESS_H

#include <arpa/inet.h>
#include <memory>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <vector>

namespace net {

class IPAddress;
class UnixAddress;

class Address {
 public:
  typedef std::shared_ptr<Address> ptr;

  virtual ~Address() {}

  static Address::ptr Create(const sockaddr* addr, socklen_t addr_len);
  static bool Lookup(std::vector<Address::ptr>& result, const std::string& host,
                     int family = AF_INET, int type = 0, int protocol = 0);
  static Address::ptr LookupAny(const std::string& host, int family = AF_INET,
                                int type = 0, int protocol = 0);
  static std::shared_ptr<IPAddress> LookupAnyIPAddress(
      const std::string& host, int family = AF_INET, int type = 0,
      int protocol = 0);
  static std::shared_ptr<UnixAddress> LookupAnyUnixAddress(
      const std::string& host, int family = AF_UNIX, int type = 0,
      int protocol = 0);

  int getFamily() const;

  virtual sockaddr* getAddr() = 0;
  virtual const sockaddr* getAddr() const = 0;
  virtual socklen_t getAddrLen() const = 0;
  virtual std::ostream& insert(std::ostream& os) const = 0;
  std::string toString() const;

  bool operator<(const Address& rhs) const;
  bool operator==(const Address& rhs) const;
  bool operator!=(const Address& rhs) const;
};

class IPAddress : public Address {
 public:
  typedef std::shared_ptr<IPAddress> ptr;

  static IPAddress::ptr Create(const char* address, uint16_t port = 0);

  virtual IPAddress::ptr broadcastAddress(uint32_t prefix_len) = 0;
  virtual IPAddress::ptr networkAddress(uint32_t prefix_len) = 0;
  virtual IPAddress::ptr subnetMask(uint32_t prefix_len) = 0;
  virtual std::ostream& insertAddr(std::ostream& os) const = 0;
  virtual std::string AddrString() const = 0;
  virtual uint16_t getPort() const = 0;
  virtual void setPort(uint16_t v) = 0;
};

class IPv4Address : public IPAddress {
 public:
  typedef std::shared_ptr<IPv4Address> ptr;

  static IPv4Address::ptr Create(const char* address, uint16_t port = 0);

  IPv4Address(uint32_t address = INADDR_ANY, uint16_t port = 0);
  explicit IPv4Address(const sockaddr_in& addr);

  std::ostream& insert(std::ostream& os) const override;
  std::ostream& insertAddr(std::ostream& os) const override;
  std::string AddrString() const override;
  const sockaddr* getAddr() const override;
  sockaddr* getAddr() override;
  socklen_t getAddrLen() const override;

  IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;
  IPAddress::ptr networkAddress(uint32_t prefix_len) override;
  IPAddress::ptr subnetMask(uint32_t prefix_len) override;

  void setPort(uint16_t v) override;
  uint16_t getPort() const override;

 private:
  sockaddr_in addr_;
};

class IPv6Address : public IPAddress {
 public:
  typedef std::shared_ptr<IPv6Address> ptr;

  static IPv6Address::ptr Create(const char* address, uint16_t port = 0);

  IPv6Address();
  explicit IPv6Address(const sockaddr_in6& addr);
  IPv6Address(const uint8_t address[16], uint16_t port = 0);

  std::ostream& insert(std::ostream& os) const override;
  std::ostream& insertAddr(std::ostream& os) const override;
  std::string AddrString() const override;
  const sockaddr* getAddr() const override;
  sockaddr* getAddr() override;
  socklen_t getAddrLen() const override;

  IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;
  IPAddress::ptr networkAddress(uint32_t prefix_len) override;
  IPAddress::ptr subnetMask(uint32_t prefix_len) override;

  void setPort(uint16_t v) override;
  uint16_t getPort() const override;

 private:
  sockaddr_in6 addr_;
};

class UnixAddress : public Address {
 public:
  typedef std::shared_ptr<UnixAddress> ptr;

  explicit UnixAddress(const std::string& path);
  UnixAddress();

  const sockaddr* getAddr() const override;
  sockaddr* getAddr() override;
  socklen_t getAddrLen() const override;
  std::ostream& insert(std::ostream& os) const override;
  void setAddrLen(uint32_t v);

 private:
  sockaddr_un addr_;
  socklen_t length_;
};

class UnknownAddress : public Address {
 public:
  typedef std::shared_ptr<UnknownAddress> ptr;

  explicit UnknownAddress(int family);
  explicit UnknownAddress(const sockaddr& addr);

  const sockaddr* getAddr() const override;
  sockaddr* getAddr() override;
  socklen_t getAddrLen() const override;
  std::ostream& insert(std::ostream& os) const override;

 private:
  sockaddr addr_;
};

std::ostream& operator<<(std::ostream& os, const Address& addr);

}  // namespace net

#endif  // NET_SOCKET_ADDRESS_H
