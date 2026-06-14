#include "lemo/server/acceptor.h"

namespace lemo {
namespace server {

Acceptor::Acceptor(io::Runtime* accept_runtime, socket::Address::ptr listen_addr)
    : accept_runtime_(accept_runtime), listen_addr_(std::move(listen_addr)) {
  listen_ = socket::Socket::CreateTCP(listen_addr_);
}

Acceptor::~Acceptor() { stop(); }

bool Acceptor::listen(int backlog) {
  if (!listen_ || !listen_addr_) {
    return false;
  }
  if (!listen_->bind(listen_addr_) || !listen_->listen(backlog)) {
    return false;
  }
  return true;
}

void Acceptor::start(io::Runtime* worker, ConnectionHandler handler) {
  if (!listen_ || !worker || !handler) {
    return;
  }
  stopped_.store(false, std::memory_order_release);
  accept_runtime_->iom().schedule([this, worker, handler]() {
    acceptLoop(worker, handler);
  });
}

void Acceptor::stop() {
  stopped_.store(true, std::memory_order_release);
  if (listen_) {
    listen_->close();
  }
}

void Acceptor::acceptLoop(io::Runtime* worker, ConnectionHandler handler) {
  while (!stopped_.load(std::memory_order_acquire)) {
    socket::Socket::ptr client = listen_->accept();
    if (!client) {
      if (stopped_.load(std::memory_order_acquire)) {
        break;
      }
      continue;
    }
    worker->iom().schedule([handler, client]() { handler(client); });
  }
}

}  // namespace server
}  // namespace lemo
