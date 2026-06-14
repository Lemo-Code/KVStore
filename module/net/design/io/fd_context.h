#ifndef NET_DESIGN_IO_FD_CONTEXT_H
#define NET_DESIGN_IO_FD_CONTEXT_H

#include <cstdint>

namespace net {

struct FdContext {
  bool is_socket = false;
  bool is_nonblock = false;
  bool is_close = false;
  uint64_t recv_timeout_ms = UINT64_MAX;
  uint64_t send_timeout_ms = UINT64_MAX;
};

class FdManager {
 public:
  static FdManager& instance();
  FdContext* get(int fd);
  void del(int fd);
};

}  // namespace net

#endif  // NET_DESIGN_IO_FD_CONTEXT_H
