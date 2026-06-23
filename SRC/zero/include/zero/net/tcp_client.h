// zero TcpClient — async TCP client with reconnection support
//
// Provides non-blocking TCP client connections with automatic retry
// and exponential backoff. When connected, provides a SocketStream
// for bidirectional I/O.
//
// Usage:
//   TcpClient client;
//   client.set_connect_callback([](SocketStream::Ptr stream) {
//       stream->write_string("hello");
//       stream->flush();
//   });
//   client.set_error_callback([](int err, const std::string& msg) {
//       // Handle connection error
//   });
//   auto addr = IPv4Address("127.0.0.1", 8080);
//   client.connect(addr, 5000);  // 5 second timeout
#pragma once

#include <memory>
#include <functional>
#include <string>
#include <atomic>

#include "zero/net/socket.h"
#include "zero/net/address.h"
#include "zero/net/socket_stream.h"

namespace zero {

class Scheduler;

class TcpClient {
public:
    using ConnectCallback = std::function<void(SocketStream::Ptr)>;
    using ErrorCallback = std::function<void(int error, const std::string& msg)>;
    using CloseCallback = std::function<void()>;

    // ============================================================
    // Construction
    // ============================================================

    TcpClient();
    ~TcpClient();

    TcpClient(const TcpClient&) = delete;
    TcpClient& operator=(const TcpClient&) = delete;

    // ============================================================
    // Configuration
    // ============================================================

    // Set the callback invoked when a connection is successfully
    // established. The SocketStream is ready for I/O.
    void set_connect_callback(ConnectCallback cb);

    // Set the error callback for connection failures.
    void set_error_callback(ErrorCallback cb);

    // Set the callback for clean disconnection.
    void set_close_callback(CloseCallback cb);

    // ============================================================
    // Connection management
    // ============================================================

    // Initiate an async connection to the given address.
    // Returns true if the connection attempt was started successfully.
    // The result is delivered via the connect/error callbacks.
    bool connect(const Address& addr, int timeout_ms = 5000);

    // Initiate an async connection using a resolved address.
    bool connect(std::shared_ptr<Address> addr, int timeout_ms = 5000);

    // ============================================================
    // Reconnection
    // ============================================================

    // Enable/disable automatic reconnection on disconnect.
    // max_retries: -1 = infinite retries, 0 = no retry, N = at most N.
    // initial_delay_ms: starting delay before first retry.
    // max_delay_ms: maximum delay between retries (caps exponential
    //   backoff).
    void set_reconnect(bool enable,
                        int max_retries = -1,
                        int initial_delay_ms = 100,
                        int max_delay_ms = 30000);

    // ============================================================
    // Lifecycle
    // ============================================================

    // Close the connection.
    void close();

    // ============================================================
    // Observers
    // ============================================================

    // Get the current stream (nullptr if not connected).
    SocketStream::Ptr stream() const noexcept { return stream_; }

    // Whether currently connected.
    bool is_connected() const noexcept { return connected_; }

    // The remote address (valid after connect())
    std::shared_ptr<Address> remote_address() const {
        return remote_addr_;
    }

    // Retry statistics
    int retry_count() const noexcept { return retry_count_; }
    int max_retries() const noexcept { return max_retries_; }

private:
    // Perform the actual connection attempt (called from fiber)
    void do_connect();

    // Schedule a retry after a delay
    void schedule_retry();

    // Handle disconnection
    void handle_disconnect();

    SocketStream::Ptr stream_;
    std::shared_ptr<Address> remote_addr_;
    int timeout_ms_ = 5000;

    ConnectCallback connect_cb_;
    ErrorCallback error_cb_;
    CloseCallback close_cb_;

    std::atomic<bool> connected_{false};
    std::atomic<bool> connecting_{false};

    // Reconnection state
    bool reconnect_ = false;
    int max_retries_ = -1;
    int retry_count_ = 0;
    int initial_delay_ms_ = 100;
    int max_delay_ms_ = 30000;
    int current_delay_ms_ = 100;

    Scheduler* scheduler_ = nullptr;
};

} // namespace zero
