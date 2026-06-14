#include "ringbuffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <type_traits>

namespace sylar {

const char Ringbuffer::kCRLF[] = "\r\n";

const size_t Ringbuffer::kCheapPrepend = 8;
const size_t Ringbuffer::kInitialSize = 1024;

template<typename To, typename From>
inline To implicit_cast(From const &f)
{
  return static_cast<To>(f);
}

ssize_t Ringbuffer::readFd(int fd, int* savedErrno)
{
  char extrabuf[65536];
  struct iovec vec[2];
  const size_t writable = writableBytes();
  vec[0].iov_base = beginWrite();
  vec[0].iov_len = writable;
  vec[1].iov_base = extrabuf;
  vec[1].iov_len = sizeof extrabuf;
  
  const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
  const ssize_t n = ::readv(fd, vec, iovcnt);
  
  if (n < 0)
  {
    *savedErrno = errno;
  }
  else if (implicit_cast<size_t>(n) <= writable)
  {
    writerIndex_ += n;
  }
  else
  {
    writerIndex_ = buffer_.size();
    append(extrabuf, n - writable);
  }
  
  return n;
}

}  // namespace sylar


