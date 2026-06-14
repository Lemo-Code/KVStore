#pragma once

#include "lemo/server/acceptor.h"
#include "ledis/config/ledis_settings.h"
#include "ledis/session/ledis_engine.h"

#include "ledis/types.h"

#include <atomic>
#include <cstdint>

namespace lemo {
namespace io {
class Runtime;
}
namespace server {
class TcpServer;
class WorkerGroup;
}
}  // namespace lemo

namespace ledis {

class IoWorker;
class Session;

/** Ledis TCP 服务端：TcpServer + WorkerGroup + Session + IO/DB 队列。 */
class LedisServer {
 public:
  explicit LedisServer(LedisSettings settings = LedisSettings());
  ~LedisServer();

  LedisServer(const LedisServer&) = delete;
  LedisServer& operator=(const LedisServer&) = delete;

  bool start();
  void stop();
  bool running() const { return running_; }
  uint16_t boundPort() const { return bound_port_; }
  const LedisSettings& settings() const { return settings_; }
  LedisEngine& engine() { return engine_; }
  lemo::io::Runtime* runtime();

  size_t activeConnections() const {
    return active_connections_.load(std::memory_order_relaxed);
  }

 private:
  void handleConnection(const lemo::socket::Socket::ptr& sock);
  uint16_t resolveBindPort() const;
  uint32_t currentIoThreadId() const;
  void runConnectionLoop(Session& session, IoWorker* worker,
                         const lemo::socket::Socket::ptr& sock, uint32_t io_tid);

  LedisSettings settings_;
  LedisEngine engine_;
  UniquePtr<lemo::server::WorkerGroup> worker_group_;
  UniquePtr<lemo::io::Runtime> accept_runtime_;
  UniquePtr<lemo::server::TcpServer> tcp_server_;
  StdVector<UniquePtr<IoWorker>> io_workers_;
  std::atomic<uint64_t> next_conn_id_{1};
  std::atomic<size_t> active_connections_{0};
  std::atomic<bool> running_{false};
  uint16_t bound_port_ = 0;
};

}  // namespace ledis
