#include "io/connect.h"
#include "io/hook_fwd.h"

namespace net {

int connectWithTimeout(int fd, const struct sockaddr* addr, socklen_t addrlen,
                       uint64_t timeout_ms) {
  return ::connect_with_timeout(fd, addr, addrlen, timeout_ms);
}

}  // namespace net
