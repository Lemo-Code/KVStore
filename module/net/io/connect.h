#ifndef NET_IO_CONNECT_H
#define NET_IO_CONNECT_H

#include <cstdint>
#include <sys/socket.h>
#include <sys/types.h>

namespace net {

/** 带超时的 connect；hook 模块就绪后可替换为协程版实现。 */
int connectWithTimeout(int fd, const struct sockaddr* addr, socklen_t addrlen,
                       uint64_t timeout_ms);

}  // namespace net

#endif  // NET_IO_CONNECT_H
