#pragma once

#include <cstdint>
#include <sys/socket.h>
#include <sys/types.h>

namespace lemo {
namespace io {

class IOManager;

bool is_hook_enable();
void set_hook_enable(bool flag);
void set_hook_iomanager(IOManager* iom);
IOManager* get_hook_iom();
void hook_init();

void set_connect_timeout(uint64_t timeout_ms);
uint64_t get_connect_timeout();

int do_connect(int fd, const ::sockaddr* addr, socklen_t addrlen,
               uint64_t timeout_ms);

}  // namespace io
}  // namespace lemo
