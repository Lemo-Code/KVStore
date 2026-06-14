#include "lemo/server/tcp_server.h"

#include "lemo/socket/address.h"

namespace lemo {
namespace server {

TcpServer::TcpServer(const std::string& name, io::Runtime* worker,
                     io::Runtime* accept_worker)
    : name_(name),
      worker_(worker),
      accept_worker_(accept_worker != nullptr ? accept_worker : worker),
      worker_group_(nullptr) {}

TcpServer::TcpServer(const std::string& name, WorkerGroup* workers,
                     io::Runtime* accept_worker)
    : name_(name),
      worker_(workers != nullptr ? &workers->worker(0) : nullptr),
      accept_worker_(accept_worker != nullptr ? accept_worker
                                              : (workers != nullptr ? &workers->worker(0)
                                                                      : nullptr)),
      worker_group_(workers) {}

bool TcpServer::bind(const socket::Address::ptr& addr) {
  if (!addr) {
    return false;
  }
  socket::Socket::ptr sock = socket::Socket::CreateTCP(addr);
  if (!sock || !sock->bind(addr) || !sock->listen()) {
    return false;
  }
  listen_socks_.push_back(sock);
  return true;
}

bool TcpServer::bind(const std::string& host, uint16_t port) {
  socket::IPv4Address::ptr addr = socket::IPv4Address::Create(host.c_str(), port);
  return bind(addr);
}

void TcpServer::setConnectionHandler(ConnectionHandler cb) {
  handler_ = std::move(cb);
}

bool TcpServer::start() {
  if (!worker_ || !accept_worker_ || listen_socks_.empty() || !handler_) {
    return false;
  }
  if (!stopped_.load(std::memory_order_acquire)) {
    return true;
  }
  stopped_.store(false, std::memory_order_release);
  for (size_t i = 0; i < listen_socks_.size(); ++i) {
    socket::Socket::ptr sock = listen_socks_[i];
    accept_worker_->iom().schedule([this, sock]() { startAccept(sock); });
  }
  return true;
}

void TcpServer::stop() {
  stopped_.store(true, std::memory_order_release);
  for (size_t i = 0; i < listen_socks_.size(); ++i) {
    if (listen_socks_[i]) {
      listen_socks_[i]->close();
    }
  }
}

void TcpServer::startAccept(const socket::Socket::ptr& sock) {
  while (!stopped_.load(std::memory_order_acquire)) {
    socket::Socket::ptr client = sock->accept();
    if (!client) {
      if (stopped_.load(std::memory_order_acquire)) {
        break;
      }
      continue;
    }

    io::Runtime* target = worker_;
    if (worker_group_ != nullptr) {
      target = &worker_group_->pick();
    }
    if (!target) {
      client->close();
      continue;
    }

    ConnectionHandler handler = handler_;
    target->iom().schedule([handler, client]() { handler(client); });
  }
}

}  // namespace server
}  // namespace lemo
