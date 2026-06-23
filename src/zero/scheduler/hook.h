#pragma once

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdint>
#include <ctime>

namespace zero {

// 是否启用 hook (线程级别)
bool IsHookEnabled();
void SetHookEnabled(bool enabled);

} // namespace zero

// ============================================================
// Hooked syscall 函数指针类型 + 实例
// ============================================================
extern "C" {

// ---- sleep 族 ----
typedef unsigned int (*sleep_func_t)(unsigned int seconds);
extern sleep_func_t sleep_f;

typedef int (*usleep_func_t)(useconds_t usec);
extern usleep_func_t usleep_f;

typedef int (*nanosleep_func_t)(const struct timespec* req, struct timespec* rem);
extern nanosleep_func_t nanosleep_f;

// ---- socket 操作 ----
typedef int (*socket_func_t)(int domain, int type, int protocol);
extern socket_func_t socket_f;

typedef int (*connect_func_t)(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
extern connect_func_t connect_f;

typedef int (*accept_func_t)(int sockfd, struct sockaddr* addr, socklen_t* addrlen);
extern accept_func_t accept_f;

// ---- read 族 ----
typedef ssize_t (*read_func_t)(int fd, void* buf, size_t count);
extern read_func_t read_f;

typedef ssize_t (*readv_func_t)(int fd, const struct iovec* iov, int iovcnt);
extern readv_func_t readv_f;

typedef ssize_t (*recv_func_t)(int sockfd, void* buf, size_t len, int flags);
extern recv_func_t recv_f;

typedef ssize_t (*recvfrom_func_t)(int sockfd, void* buf, size_t len, int flags,
                                    struct sockaddr* src_addr, socklen_t* addrlen);
extern recvfrom_func_t recvfrom_f;

typedef ssize_t (*recvmsg_func_t)(int sockfd, struct msghdr* msg, int flags);
extern recvmsg_func_t recvmsg_f;

// ---- write 族 ----
typedef ssize_t (*write_func_t)(int fd, const void* buf, size_t count);
extern write_func_t write_f;

typedef ssize_t (*writev_func_t)(int fd, const struct iovec* iov, int iovcnt);
extern writev_func_t writev_f;

typedef ssize_t (*send_func_t)(int sockfd, const void* buf, size_t len, int flags);
extern send_func_t send_f;

typedef ssize_t (*sendto_func_t)(int sockfd, const void* buf, size_t len, int flags,
                                  const struct sockaddr* dest_addr, socklen_t addrlen);
extern sendto_func_t sendto_f;

typedef ssize_t (*sendmsg_func_t)(int sockfd, const struct msghdr* msg, int flags);
extern sendmsg_func_t sendmsg_f;

// ---- 控制操作 ----
typedef int (*close_func_t)(int fd);
extern close_func_t close_f;

typedef int (*fcntl_func_t)(int fd, int cmd, ...);
extern fcntl_func_t fcntl_f;

typedef int (*ioctl_func_t)(int fd, unsigned long request, ...);
extern ioctl_func_t ioctl_f;

typedef int (*getsockopt_func_t)(int sockfd, int level, int optname,
                                  void* optval, socklen_t* optlen);
extern getsockopt_func_t getsockopt_f;

typedef int (*setsockopt_func_t)(int sockfd, int level, int optname,
                                  const void* optval, socklen_t optlen);
extern setsockopt_func_t setsockopt_f;

} // extern "C"
