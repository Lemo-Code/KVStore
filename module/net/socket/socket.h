#ifndef NET_SOCKET_SOCKET_H
#define NET_SOCKET_SOCKET_H

#include "socket/address.h"
#include "thread/noncopyable.h"

#include <cstdint>
#include <memory>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/uio.h>

namespace net {

class Socket : public std::enable_shared_from_this<Socket>, Noncopyable {
 public:
  enum Type {
    TCP = SOCK_STREAM,
    UDP = SOCK_DGRAM,
  };

  enum Family {
    IPv4 = AF_INET,
    IPv6 = AF_INET6,
    UNIX = AF_UNIX,
  };

  typedef std::shared_ptr<Socket> ptr;

  static Socket::ptr CreateTCP(Address::ptr address);
  static Socket::ptr CreateUDP(Address::ptr address);

  static Socket::ptr CreateTCPSocket();
  static Socket::ptr CreateUDPSocket();
  static Socket::ptr CreateTCPSocket6();
  static Socket::ptr CreateUDPSocket6();
  static Socket::ptr CreateUnixTCPSocket();
  static Socket::ptr CreateUnixUDPSocket();

  Socket(int family, int type, int protocol = 0);
  ~Socket();

  int64_t getSendTimeout();
  void setSendTimeout(int64_t v);
  int64_t getRecvTimeout();
  void setRecvTimeout(int64_t v);

  bool getOption(int level, int option, void* result, size_t* len);
  template <class T>
  bool getOption(int level, int option, T& result) {
    size_t length = sizeof(T);
    return getOption(level, option, &result, &length);
  }

  bool setOption(int level, int option, const void* result, size_t len);
  template <class T>
  bool setOption(int level, int option, const T& value) {
    return setOption(level, option, &value, sizeof(T));
  }

  virtual Socket::ptr accept();
  virtual bool init(int sock);
  virtual bool bind(const Address::ptr addr);
  virtual bool connect(const Address::ptr addr, uint64_t timeout_ms = UINT64_MAX);
  virtual bool listen(int backlog = SOMAXCONN);
  virtual bool close();
  virtual bool isClose() const;

  virtual int send(const void* buffers, size_t length, int flags = 0);
  virtual int send(const iovec* buffers, size_t length, int flags = 0);
  virtual int sendTo(const void* buffers, size_t length, const Address::ptr to,
                     int flags = 0);
  virtual int sendTo(const iovec* buffers, size_t length, const Address::ptr to,
                     int flags = 0);

  virtual int recv(void* buffer, size_t length, int flags = 0);
  virtual int recv(iovec* buffer, size_t length, int flags = 0);
  virtual int recvFrom(void* buffer, size_t length, Address::ptr from,
                       int flags = 0);
  virtual int recvFrom(iovec* buffer, size_t length, Address::ptr from,
                       int flags = 0);

  Address::ptr getRemoteAddress();
  Address::ptr getLocalAddress();

  int getFamily() const { return family_; }
  int getType() const { return type_; }
  int getProtocol() const { return protocol_; }

  bool isConnected() const { return is_connected_; }
  bool isValid() const;
  int getError();

  virtual std::ostream& dump(std::ostream& os) const;
  int getSocket() const { return sock_; }

  bool cancelRead();
  bool cancelWrite();
  bool cancelAccept();
  bool cancelAll();

 private:
  void initSock();
  void newSock();

 protected:
  int sock_ = -1;
  int family_ = 0;
  int type_ = 0;
  int protocol_ = 0;
  bool is_connected_ = false;

  Address::ptr local_address_;
  Address::ptr remote_address_;
};

std::ostream& operator<<(std::ostream& os, const Socket& sock);

}  // namespace net

#endif  // NET_SOCKET_SOCKET_H
