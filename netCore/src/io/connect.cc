#include "lemo/io/connect.h"
#include "lemo/io/hook_fwd.h"

namespace lemo {
namespace io {

int connectWithTimeout(int fd, const struct sockaddr* addr, socklen_t addrlen,
                       uint64_t timeout_ms) {
  return ::connect_with_timeout(fd, addr, addrlen, timeout_ms);
}

}  // namespace io
}  // namespace lemo
