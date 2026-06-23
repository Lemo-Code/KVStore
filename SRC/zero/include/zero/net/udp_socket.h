// zero UdpSocket — UDP socket with multicast and fiber-aware I/O
//
// Wraps a UDP socket with support for:
//   - Bind to local address/port
//   - Send to / recv from specific addresses
//   - Connected UDP (set default destination)
//   - Broadcast mode
//   - Multicast: join/leave groups, set TTL, set loopback
//   - Source-specific multicast (SSM)
//   - IPv4 and IPv6 support
//   - Fiber-aware non-blocking I/O (yields on EAGAIN)
//   - Scatter/gather via sendmsg/recvmsg
//
// Usage:
//   auto sock = UdpSocket::Create();
//   sock->Bind(IPv4Address(9090));
//   sock->SetBroadcast(true);
//
//   std::shared_ptr<Address> from;
//   char buf[65536];
//   ssize_t n = sock->RecvFrom(buf, sizeof(buf), from);
//   sock->SendTo(*from, "ACK", 3);
#pragma once

#include <memory>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>

#include "zero/net/address.h"
#include "zero/base/noncopyable.h"

namespace zero {

class UdpSocket : public Noncopyable,
                  public std::enable_shared_from_this<UdpSocket> {
public:
    using Ptr = std::shared_ptr<UdpSocket>;

    // ============================================================
    // Factory methods
    // ============================================================

    // Create a new unbound UDP socket (IPv4 by default).
    static Ptr Create(int family = AF_INET);

    // Create a UDP socket bound to the given address.
    static Ptr CreateBound(const Address& addr);

    // Create a UdpSocket wrapping an existing file descriptor.
    // Takes ownership of the fd.
    static Ptr FromFd(int fd);

    ~UdpSocket();

    // ============================================================
    // Binding and connecting
    // ============================================================

    // Bind to a local address.
    bool Bind(const Address& addr);

    // Bind to any interface on the given port.
    bool Bind(uint16_t port);

    // Connect to a remote address (sets default destination for
    // Send/Recv; also filters datagrams from other sources).
    bool Connect(const Address& addr);

    // ============================================================
    // Send operations
    // ============================================================

    // Send data to a specific address.
    ssize_t SendTo(const Address& addr, const void* data, size_t size);

    // Send data using raw sockaddr (for custom address types).
    ssize_t SendTo(const struct sockaddr* addr, socklen_t addrlen,
                    const void* data, size_t size);

    // Send data to the connected default destination.
    ssize_t Send(const void* data, size_t size, int flags = 0);

    // Send a scatter/gather message.
    ssize_t SendMsg(const struct msghdr& msg, int flags = 0);

    // ============================================================
    // Receive operations
    // ============================================================

    // Receive data and capture the sender's address.
    ssize_t RecvFrom(void* buffer, size_t size,
                      std::shared_ptr<Address>& out_addr);

    // Receive data with raw sockaddr output.
    ssize_t RecvFrom(struct sockaddr* addr, socklen_t* addrlen,
                      void* buffer, size_t size);

    // Receive data from the connected default peer.
    ssize_t Recv(void* buffer, size_t size, int flags = 0);

    // Receive with ancillary data (CMSG, TOS, timestamp, etc.).
    ssize_t RecvMsg(struct msghdr& msg, int flags = 0);

    // ============================================================
    // Socket options
    // ============================================================

    bool SetBroadcast(bool enable);
    bool SetReuseAddr(bool enable);
    bool SetReusePort(bool enable);
    bool SetNonBlocking(bool enable);
    bool SetSendBufferSize(int size_bytes);
    bool SetRecvBufferSize(int size_bytes);

    int GetRecvBufferSize() const;
    int GetSendBufferSize() const;
    bool IsNonBlocking() const;

    // ============================================================
    // Multicast options
    // ============================================================

    // Set the time-to-live for multicast packets.
    bool SetMulticastTTL(int ttl);

    // Enable/disable multicast loopback (default enabled).
    bool SetMulticastLoop(bool enable);

    // Set the outgoing multicast interface by local address.
    bool SetMulticastInterface(const Address& interface_addr);

    // Join a multicast group on the given interface.
    bool JoinMulticastGroup(const Address& group_addr,
                             const Address* interface_addr = nullptr);

    // Leave a multicast group.
    bool LeaveMulticastGroup(const Address& group_addr,
                              const Address* interface_addr = nullptr);

    // Join a source-specific multicast group (SSM, RFC 3569).
    bool JoinSourceSpecificMulticast(const Address& group_addr,
                                      const Address& source_addr,
                                      const Address* interface_addr = nullptr);

    // ============================================================
    // Observers
    // ============================================================

    int GetFd() const noexcept { return fd_; }
    bool IsOpen() const noexcept { return fd_ >= 0; }
    bool IsBound() const noexcept { return bound_; }

    // Get the local address this socket is bound to.
    std::shared_ptr<Address> GetLocalAddress() const;

    // Get the connected peer address (if Connect was called).
    std::shared_ptr<Address> GetPeerAddress() const;

    // Get the last socket error (SO_ERROR).
    int GetError() const noexcept;

    // Close the socket. Safe to call multiple times.
    void Close();

private:
    // Private constructor — use factory methods.
    explicit UdpSocket(int fd) : fd_(fd) {}

    // Helper: fill sockaddr_storage from an Address for multicast ops.
    static bool fill_sockaddr(const Address& addr,
                               struct sockaddr_storage& out,
                               socklen_t& out_len);

    // Helper: set a multicast socket option with level-dependent option name.
    bool set_multicast_opt(int level, int optname,
                            const void* optval, socklen_t optlen);

    int fd_ = -1;
    bool bound_ = false;
};

} // namespace zero
