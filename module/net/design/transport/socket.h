#ifndef NET_DESIGN_TRANSPORT_SOCKET_H
#define NET_DESIGN_TRANSPORT_SOCKET_H

#include <cstdint>
#include <memory>

namespace net {

class Address;

class Socket : public std::enable_shared_from_this<Socket> {
 public:
  using ptr = std::shared_ptr<Socket>;

  static ptr CreateTCP(Address* addr);
  static ptr CreateUDP(Address* addr);

  virtual bool bind(const Address* addr) = 0;
  virtual bool connect(const Address* addr, uint64_t timeout_ms = UINT64_MAX) = 0;
  virtual bool listen(int backlog) = 0;
  virtual ptr accept() = 0;
  virtual bool close() = 0;

  virtual int send(const void* buf, size_t len, int flags = 0) = 0;
  virtual int recv(void* buf, size_t len, int flags = 0) = 0;

  virtual int fd() const = 0;
};

}  // namespace net

#endif  // NET_DESIGN_TRANSPORT_SOCKET_H
