#ifndef NET_IO_HOOK_H
#define NET_IO_HOOK_H

#include <cstdint>

namespace net {

bool is_hook_enable();
void set_hook_enable(bool flag);
void hook_init();

/** connect hook 默认超时（毫秒），UINT64_MAX 表示无限等待 */
void set_connect_timeout(uint64_t timeout_ms);
uint64_t get_connect_timeout();

}  // namespace net

#endif  // NET_IO_HOOK_H
