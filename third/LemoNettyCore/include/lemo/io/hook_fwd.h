#pragma once

#include "lemo/io/hook.h"

#include <cstdint>
#include <sys/socket.h>
#include <sys/types.h>

extern "C" {

typedef int (*fcntl_fun)(int fd, int cmd, ...);
extern fcntl_fun fcntl_f;

int connect_with_timeout(int fd, const struct sockaddr* addr, socklen_t addrlen,
                         uint64_t timeout_ms);

}  // extern "C"
