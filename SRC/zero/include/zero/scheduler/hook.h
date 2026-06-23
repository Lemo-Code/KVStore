// zero syscall hooks — transparent async I/O via dlsym interposition
#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#include <ctime>
#include <cstdarg>

namespace zero {

bool is_hook_enabled() noexcept;
void set_hook_enabled(bool enabled) noexcept;

unsigned int sleep(unsigned int seconds);
int usleep(useconds_t usec);
int nanosleep(const struct timespec* req, struct timespec* rem);

int socket(int domain, int type, int protocol);
int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen);
int accept4(int sockfd, struct sockaddr* addr, socklen_t* addrlen, int flags);

ssize_t read(int fd, void* buf, size_t count);
ssize_t readv(int fd, const struct iovec* iov, int iovcnt);
ssize_t pread(int fd, void* buf, size_t count, off_t offset);
ssize_t recv(int sockfd, void* buf, size_t len, int flags);
ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags,
                 struct sockaddr* src_addr, socklen_t* addrlen);
ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags);

ssize_t write(int fd, const void* buf, size_t count);
ssize_t writev(int fd, const struct iovec* iov, int iovcnt);
ssize_t pwrite(int fd, const void* buf, size_t count, off_t offset);
ssize_t send(int sockfd, const void* buf, size_t len, int flags);
ssize_t sendto(int sockfd, const void* buf, size_t len, int flags,
               const struct sockaddr* dest_addr, socklen_t addrlen);
ssize_t sendmsg(int sockfd, const struct msghdr* msg, int flags);

int close(int fd);
int fcntl(int fd, int cmd, ...);
int ioctl(int fd, unsigned long request, ...);
int getsockopt(int sockfd, int level, int optname,
               void* optval, socklen_t* optlen);
int setsockopt(int sockfd, int level, int optname,
               const void* optval, socklen_t optlen);

} // namespace zero
