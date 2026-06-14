#pragma once

#include "lemo/io/runtime.h"
#include "lemo/nettycore_export.h"
#include "lemo/socket/address.h"
#include "lemo/socket/socket.h"

#include <atomic>
#include <functional>
#include <memory>

namespace lemo {
namespace server {

using ConnectionHandler = std::function<void(socket::Socket::ptr)>;

/**
 * @brief 监听套接字 accept 循环，将新连接分发到 worker Runtime。
 */
class LEMO_NETTYCORE_API Acceptor {
 public:
  typedef std::shared_ptr<Acceptor> ptr;

  Acceptor(io::Runtime* accept_runtime, socket::Address::ptr listen_addr);
  ~Acceptor();

  Acceptor(const Acceptor&) = delete;
  Acceptor& operator=(const Acceptor&) = delete;

  bool listen(int backlog = 128);
  void start(io::Runtime* worker, ConnectionHandler handler);
  void stop();

  socket::Socket::ptr listenSocket() const { return listen_; }

 private:
  void acceptLoop(io::Runtime* worker, ConnectionHandler handler);

  io::Runtime* accept_runtime_;
  socket::Address::ptr listen_addr_;
  socket::Socket::ptr listen_;
  std::atomic<bool> stopped_{true};
};

}  // namespace server
}  // namespace lemo
