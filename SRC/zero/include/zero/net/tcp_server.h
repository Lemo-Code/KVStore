// zero TcpServer — multi-threaded TCP server
//
// A high-performance TCP server using SO_REUSEPORT for multi-threaded
// accept (one listen socket per worker thread). When a fiber-based
// scheduler is active, connections are handled as fibers on the worker
// threads, providing lightweight concurrency without thread-per-connection
// overhead.
//
// Usage:
//   TcpServer server;
//   server.set_connection_callback([](SocketStream::Ptr conn) {
//       // Handle connection in a fiber
//       std::string data = conn->read_all();
//       conn->write_string("HTTP/1.1 200 OK\r\n\r\nHello");
//       conn->flush();
//   });
//   auto addr = IPv4Address("0.0.0.0", 8080);
//   server.bind(addr);
//   server.start();  // Begins accepting connections
//   server.stop();   // Graceful shutdown
#pragma once

#include <functional>
#include <memory>
#include <atomic>
#include <vector>
#include <string>

#include "zero/net/socket.h"
#include "zero/net/address.h"
#include "zero/net/socket_stream.h"

namespace zero {

class Scheduler;

class TcpServer {
public:
    using ConnectionCallback = std::function<void(SocketStream::Ptr)>;
    using ErrorCallback = std::function<void(int error, const std::string& msg)>;

    // ============================================================
    // Construction
    // ============================================================

    TcpServer();
    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    // ============================================================
    // Configuration
    // ============================================================

    // Set the callback invoked when a new connection is accepted.
    // The callback receives a SocketStream wrapping the new socket.
    void set_connection_callback(ConnectionCallback cb);

    // Set the error callback for accept/listen errors.
    void set_error_callback(ErrorCallback cb);

    // Set the listen backlog (default 128).
    void set_backlog(int backlog) noexcept { backlog_ = backlog; }

    // Set the number of worker threads (default: all CPUs).
    void set_worker_count(size_t n) noexcept { worker_count_ = n; }

    // Enable/disable SO_REUSEPORT (default enabled).
    void set_reuse_port(bool on) noexcept { reuse_port_ = on; }

    // Set the name for worker threads
    void set_name(const std::string& name) noexcept { name_ = name; }

    // ============================================================
    // Lifecycle
    // ============================================================

    // Bind to the given address. Must be called before start().
    // Returns true on success.
    bool bind(const Address& addr);

    // Bind to multiple addresses (listen on IPv4 + IPv6, for example).
    bool bind(const std::vector<std::shared_ptr<Address>>& addrs);

    // Start accepting connections.
    // If a scheduler is active, connections are handled as fibers.
    // Otherwise, calls are dispatched directly on the accept thread.
    // Returns true on success.
    bool start();

    // Stop accepting. Blocks until all outstanding connections
    // are drained (or a timeout).
    // If graceful is true, drain existing connections before closing.
    void stop(bool graceful = true);

    // ============================================================
    // Observers
    // ============================================================

    bool is_running() const noexcept { return running_; }

    // Number of currently active connections (approximate)
    size_t connection_count() const noexcept {
        return connection_count_.load(std::memory_order_relaxed);
    }

    // The listen address
    std::shared_ptr<Address> listen_address() const {
        return listen_addr_;
    }

    // The listen socket
    Socket::Ptr listen_socket() const { return listen_socket_; }

private:
    // Accept loop (runs in each worker thread's main fiber)
    void accept_loop();

    // Handle a newly accepted connection
    void handle_connection(Socket::Ptr client, int worker_id);

    Socket::Ptr listen_socket_;
    std::shared_ptr<Address> listen_addr_;
    std::vector<std::shared_ptr<Address>> listen_addrs_;

    ConnectionCallback connection_cb_;
    ErrorCallback error_cb_;

    int backlog_ = 128;
    size_t worker_count_ = 0;  // 0 = auto-detect
    bool reuse_port_ = true;
    std::string name_ = "tcp-server";

    std::atomic<bool> running_{false};
    std::atomic<size_t> connection_count_{0};

    Scheduler* scheduler_ = nullptr;
};

} // namespace zero
