#pragma once

#include <memory>
#include <functional>
#include <string>

#include "zero/net/socket.h"
#include "zero/net/address.h"
#include "zero/scheduler/scheduler.h"

namespace zero {

// ============ TcpServer ============
class TcpServer : public std::enable_shared_from_this<TcpServer> {
public:
    using ptr = std::shared_ptr<TcpServer>;

    TcpServer(Scheduler* scheduler, Address::ptr addr,
              const std::string& name = "tcpserver");
    ~TcpServer();

    bool start();
    void stop();

    // 设置新连接回调: void(Socket::ptr)
    void setConnectionCallback(std::function<void(Socket::ptr)> cb) {
        conn_cb_ = std::move(cb);
    }

    Scheduler* getScheduler() const { return scheduler_; }
    Address::ptr getAddress() const { return addr_; }

private:
    void acceptLoop();

    Scheduler* scheduler_;
    Address::ptr addr_;
    Socket::ptr listen_sock_;
    std::string name_;
    bool running_ = false;

    std::function<void(Socket::ptr)> conn_cb_;
};

} // namespace zero
