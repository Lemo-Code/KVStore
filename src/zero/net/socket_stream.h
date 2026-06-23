#pragma once

#include "zero/net/stream.h"
#include "zero/net/socket.h"

namespace zero {

// ============ SocketStream ============
class SocketStream : public Stream {
public:
    using ptr = std::shared_ptr<SocketStream>;

    explicit SocketStream(Socket::ptr sock);
    ~SocketStream() override;

    ssize_t read(void* buf, size_t len) override;
    ssize_t write(const void* buf, size_t len) override;
    void    close() override;

    Socket::ptr getSocket() const { return sock_; }

private:
    Socket::ptr sock_;
};

} // namespace zero
