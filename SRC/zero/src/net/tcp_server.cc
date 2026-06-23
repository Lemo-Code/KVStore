// zero TcpServer implementation
//
// Multi-threaded TCP server with SO_REUSEPORT support.
// Binds to a given address, listens for connections, and calls
// the user-provided connection callback for each accepted client.
//
// Integration with the scheduler:
//   - The accept loop runs as a fiber, yielding when no connections
//     are pending (instead of blocking the thread)
//   - Each accepted connection can be handled in its own fiber
//   - Connection counting and graceful shutdown are supported
//
// SO_REUSEPORT allows multiple server instances to bind to the
// same port (for multi-process or multi-thread architectures).
#include "zero/net/tcp_server.h"
#include "zero/scheduler/scheduler.h"
#include <unistd.h>
#include <cerrno>

namespace zero {

// ============================================================
// Construction / Destruction
// ============================================================
TcpServer::TcpServer() = default;

TcpServer::~TcpServer() {
    stop();
}

// ============================================================
// Configuration
// ============================================================
void TcpServer::set_connection_callback(ConnectionCallback cb) {
    connection_cb_ = std::move(cb);
}

// ============================================================
// Bind and listen
// ============================================================
bool TcpServer::bind(const Address& addr) {
    if (listen_socket_) {
        listen_socket_->close();
    }

    // Create TCP listen socket
    listen_socket_ = Socket::create_tcp();
    if (!listen_socket_) return false;

    // Configure for server use
    if (!listen_socket_->set_reuse_addr(true)) {
        listen_socket_->close();
        listen_socket_.reset();
        return false;
    }

    if (!listen_socket_->set_reuse_port(true)) {
        // SO_REUSEPORT may not be supported — non-fatal
    }

    if (!listen_socket_->set_nonblocking(true)) {
        listen_socket_->close();
        listen_socket_.reset();
        return false;
    }

    // Bind to the requested address
    if (!listen_socket_->bind(addr)) {
        listen_socket_->close();
        listen_socket_.reset();
        return false;
    }

    // Start listening
    if (!listen_socket_->listen(1024)) {
        listen_socket_->close();
        listen_socket_.reset();
        return false;
    }

    return true;
}

// ============================================================
// Start accepting connections
// ============================================================
bool TcpServer::start() {
    if (!listen_socket_ || running_) return false;

    auto* sched = Scheduler::GetThis();
    if (!sched) {
        // No scheduler running — cannot start the accept loop
        return false;
    }

    running_ = true;

    // Schedule the accept loop as a fiber on the current scheduler
    sched->schedule([this]() {
        accept_loop();
    });

    return true;
}

// ============================================================
// Stop
// ============================================================
void TcpServer::stop(bool graceful) {
    running_ = false;

    // Close the listen socket to unblock the accept() call
    if (listen_socket_) {
        listen_socket_->close();
        listen_socket_.reset();
    }
}

// ============================================================
// Accept loop (runs as a fiber)
// ============================================================
void TcpServer::accept_loop() {
    while (running_) {
        // Accept a new connection
        auto client = listen_socket_->accept();

        if (!client) {
            // No pending connections or error
            if (!running_) break;

            // Yield the fiber so the scheduler can do other work
            // (or handle the EAGAIN via I/O hooks)
            Fiber::GetThis()->yield();
            continue;
        }

        // Configure the client socket
        client->set_tcp_nodelay(true);
        client->set_nonblocking(true);
        client->set_keepalive(true);

        // Create the stream wrapper
        auto stream = std::make_shared<SocketStream>(std::move(client));

        // Invoke the user callback (which typically schedules a new
        // fiber to handle this connection)
        if (connection_cb_) {
            connection_cb_(std::move(stream));
        }
    }
}

} // namespace zero
