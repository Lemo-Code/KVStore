#ifndef NET_DESIGN_IO_HOOK_H
#define NET_DESIGN_IO_HOOK_H

#include <cstdint>

namespace net {

void hook_init();
bool is_hook_enable();
void set_hook_enable(bool flag);

void set_connect_timeout(uint64_t timeout_ms);
uint64_t get_connect_timeout();

}  // namespace net

#endif  // NET_DESIGN_IO_HOOK_H
