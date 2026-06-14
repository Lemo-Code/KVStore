#pragma once

#include "lemo/nettycore_export.h"
#include "lemo/server/acceptor.h"
#include "lemo/server/worker_group.h"
#include "lemo/socket/address.h"
#include "lemo/socket/socket.h"

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace lemo {
namespace server {

/**
 * @brief TCP 服务端：bind + listen + accept + 连接分发。
 */
class LEMO_NETTYCORE_API TcpServer {
 public:
  typedef std::shared_ptr<TcpServer> ptr;

  TcpServer(const std::string& name, io::Runtime* worker,
            io::Runtime* accept_worker = nullptr);
  TcpServer(const std::string& name, WorkerGroup* workers,
            io::Runtime* accept_worker = nullptr);

  bool bind(const socket::Address::ptr& addr);
  bool bind(const std::string& host, uint16_t port);
  bool start();
  void stop();

  void setConnectionHandler(ConnectionHandler cb);
  const std::string& name() const { return name_; }
  bool isStopped() const { return stopped_.load(std::memory_order_acquire); }

 private:
  void startAccept(const socket::Socket::ptr& sock);

  std::string name_;
  io::Runtime* worker_;
  io::Runtime* accept_worker_;
  WorkerGroup* worker_group_;
  std::vector<socket::Socket::ptr> listen_socks_;
  ConnectionHandler handler_;
  std::atomic<bool> stopped_{true};
};

}  // namespace server
}  // namespace lemo
