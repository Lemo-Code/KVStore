#pragma once

#include <cstdint>
#include <sys/socket.h>
#include <sys/types.h>

namespace lemo {
namespace io {

int connectWithTimeout(int fd, const struct sockaddr* addr, socklen_t addrlen,
                       uint64_t timeout_ms);

}  // namespace io
}  // namespace lemo
