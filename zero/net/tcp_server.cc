#include "zero/net/tcp_server.h"
#include "zero/log/log.h"
#include "zero/fiber/fiber.h"
#include <cstdio>

namespace zero {

static Logger::ptr g_logger = ZERO_LOG_NAME("tcp_server");

TcpServer::TcpServer(Scheduler* scheduler, Address::ptr addr,
                     const std::string& name)
    : scheduler_(scheduler), addr_(std::move(addr)), name_(name) {}

TcpServer::~TcpServer() { stop(); }

bool TcpServer::start() {
    if (running_) return true;

    listen_sock_ = Socket::CreateTCPSocket(addr_->getFamily());
    if (!listen_sock_) {
        ZERO_LOG_ERROR(g_logger) << "Failed to create socket";
        return false;
    }

    int val = 1;
    listen_sock_->setOption(SOL_SOCKET, SO_REUSEADDR, val);

    if (!listen_sock_->bind(addr_) || !listen_sock_->listen(SOMAXCONN)) {
        ZERO_LOG_ERROR(g_logger) << "bind/listen failed on " << addr_->toString();
        return false;
    }

    running_ = true;
    auto local = listen_sock_->getLocalAddress();
    ZERO_LOG_INFO(g_logger) << "Listening on " << (local ? local->toString() : "?");

    scheduler_->schedule(std::bind(&TcpServer::acceptLoop, shared_from_this()));

    return true;
}

void TcpServer::stop() {
    if (!running_) return;
    running_ = false;
    if (listen_sock_) {
        listen_sock_->close();
        listen_sock_.reset();
    }
}

void TcpServer::acceptLoop() {
    while (running_) {
        Socket::ptr client = listen_sock_->accept();
        if (!client) {
            if (!running_) break;
            continue;
        }

        (void)client; // silence unused warning

        // 为每个连接创建 fiber
        if (conn_cb_) {
            auto cb = conn_cb_;
            scheduler_->schedule([cb, client]() mutable {
                cb(std::move(client));
            });
        }
    }
}

} // namespace zero
