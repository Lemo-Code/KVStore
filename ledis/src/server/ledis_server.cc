#include "ledis/server/ledis_server.h"

#include "ledis/session/io_worker.h"
#include "ledis/session/session.h"
#include "ledis/types.h"

#include "lemo/fiber/fiber.h"
#include <chrono>
#include "lemo/io/runtime.h"
#include "lemo/server/tcp_server.h"
#include "lemo/server/worker_group.h"
#include "lemo/socket/address.h"
#include "lemo/socket/socket.h"

#include <cerrno>

namespace ledis {

LedisServer::LedisServer(LedisSettings settings)
    : settings_(Move(settings)), engine_(settings_) {}

LedisServer::~LedisServer() { stop(); }

lemo::io::Runtime* LedisServer::runtime() {
  if (worker_group_ && worker_group_->size() > 0) {
    return &worker_group_->worker(0);
  }
  return accept_runtime_.get();
}

uint16_t LedisServer::resolveBindPort() const {
  if (settings_.port != 0) {
    return settings_.port;
  }
  lemo::socket::Socket::ptr probe = lemo::socket::Socket::CreateTCPSocket();
  lemo::socket::IPv4Address::ptr addr =
      lemo::socket::IPv4Address::Create(settings_.host.c_str(), 0);
  if (!probe || !addr || !probe->bind(addr) || !probe->listen()) {
    return 0;
  }
  lemo::socket::Address::ptr la = probe->getLocalAddress();
  lemo::socket::IPv4Address::ptr v4 =
      std::dynamic_pointer_cast<lemo::socket::IPv4Address>(la);
  const uint16_t port = v4 ? v4->getPort() : 0;
  probe->close();
  return port;
}

uint32_t LedisServer::currentIoThreadId() const {
  if (!worker_group_) {
    return 0;
  }
  lemo::io::Runtime* rt = lemo::io::Runtime::GetThis();
  if (!rt) {
    return 0;
  }
  for (size_t i = 0; i < worker_group_->size(); ++i) {
    if (&worker_group_->worker(i) == rt) {
      return static_cast<uint32_t>(i);
    }
  }
  return 0;
}

bool LedisServer::start() {
  if (running_) {
    return true;
  }

  bound_port_ = resolveBindPort();
  if (bound_port_ == 0) {
    return false;
  }

  const uint32_t io_threads =
      settings_.io_threads > 0 ? settings_.io_threads : 1;

  worker_group_.reset(
      new lemo::server::WorkerGroup(io_threads, false, "ledis-io"));
  accept_runtime_.reset(new lemo::io::Runtime(1, false, "ledis-accept"));

  io_workers_.clear();
  io_workers_.reserve(io_threads);
  for (uint32_t i = 0; i < io_threads; ++i) {
    io_workers_.push_back(UniquePtr<IoWorker>(new IoWorker(i, &engine_)));
  }

  if (!settings_.single_thread_mode) {
    engine_.startDbWorker();
  }

  tcp_server_.reset(new lemo::server::TcpServer("ledis", worker_group_.get(),
                                                accept_runtime_.get()));
  if (!tcp_server_->bind(settings_.host, bound_port_)) {
    stop();
    return false;
  }

  tcp_server_->setConnectionHandler(
      [this](const lemo::socket::Socket::ptr& sock) { handleConnection(sock); });

  if (!tcp_server_->start()) {
    stop();
    return false;
  }

  running_ = true;
  engine_.registry().setServerConfig(bound_port_, settings_.maxclients, 0,
                                     settings_.maxmemory_policy);
  return true;
}

void LedisServer::stop() {
  running_ = false;
  if (tcp_server_) {
    tcp_server_->stop();
    tcp_server_.reset();
  }
  engine_.stopDbWorker();
  io_workers_.clear();
  if (worker_group_) {
    worker_group_->stop();
    worker_group_.reset();
  }
  if (accept_runtime_) {
    accept_runtime_->stop();
    accept_runtime_.reset();
  }
}

void LedisServer::runConnectionLoop(Session& session, IoWorker* worker,
                                    const lemo::socket::Socket::ptr& sock,
                                    uint32_t io_tid) {
  const int fd = sock->getSocket();

  auto flushOnce = [&]() {
    if (worker) {
      worker->pollReplies();
    }
    session.pollReplies(fd);
  };

  auto flushUntilIdle = [&]() {
    if (settings_.single_thread_mode) {
      flushOnce();
      return;
    }
    for (int i = 0; i < 2000; ++i) {
      flushOnce();
      if (session.outstandingCommands() == 0 &&
          session.pendingReplyCount() == 0 &&
          engine_.replyRouter().empty(io_tid)) {
        return;
      }
      lemo::fiber::Fiber::YieldToReady();
    }
  };

  while (running_ && sock->isConnected() && !sock->isClose()) {
    if (session.queryBufferExceeded()) {
      session.writeResult(fd, CommandResult::error("ERR query buffer limit"));
      break;
    }

    int saved_errno = 0;
    const ssize_t n = session.readMore(fd, 65536, &saved_errno);
    if (n > 0) {
      session.parseAndEnqueue(fd);
      flushUntilIdle();
      continue;
    }
    if (n == 0) {
      break;
    }
    if (saved_errno == EAGAIN || saved_errno == EWOULDBLOCK) {
      flushUntilIdle();
      (void)engine_.blockingLists().expireTimeouts(
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now().time_since_epoch())
              .count());
      lemo::fiber::Fiber::YieldToReady();
      continue;
    }
    break;
  }
  flushUntilIdle();
}

void LedisServer::handleConnection(const lemo::socket::Socket::ptr& sock) {
  if (!sock || !running_) {
    return;
  }

  if (settings_.maxclients > 0) {
    const size_t prev = active_connections_.fetch_add(1);
    if (prev >= engine_.maxclients()) {
      active_connections_.fetch_sub(1);
      sock->close();
      return;
    }
  } else {
    active_connections_.fetch_add(1);
  }

  const uint64_t conn_id = next_conn_id_.fetch_add(1);
  const uint32_t io_tid = currentIoThreadId();
  IoWorker* worker =
      io_tid < io_workers_.size() ? io_workers_[io_tid].get() : nullptr;

  Session session(conn_id, io_tid, &engine_);
  session.context().conn_id = conn_id;
  session.context().io_thread_id = io_tid;
  if (settings_.single_thread_mode) {
    session.setHandler([this, &session](const SessionContext&,
                                          const Command& cmd) {
      return engine_.dispatchSync(session.context(), cmd);
    });
  }
  session.stream().setQueryBufferLimit(settings_.query_buffer_limit);

  if (worker) {
    worker->registerSession(&session);
  }

  runConnectionLoop(session, worker, sock, io_tid);

  if (worker) {
    worker->unregisterSession(conn_id);
  } else {
    engine_.pubsub().disconnect(conn_id);
    engine_.blockingLists().disconnect(conn_id);
  }
  sock->close();
  active_connections_.fetch_sub(1);
}

}  // namespace ledis
