#ifndef NET_IO_HOOK_FWD_H
#define NET_IO_HOOK_FWD_H

#include "io/hook.h"

#include <cstdint>
#include <sys/socket.h>
#include <sys/types.h>

namespace net {

bool is_hook_enable();
void set_hook_enable(bool flag);

}  // namespace net

extern "C" {

typedef int (*fcntl_fun)(int fd, int cmd, ...);
extern fcntl_fun fcntl_f;

extern int connect_with_timeout(int fd, const struct sockaddr* addr,
                                socklen_t addrlen, uint64_t timeout_ms);

}  // extern "C"

#endif  // NET_IO_HOOK_FWD_H
