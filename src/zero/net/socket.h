#pragma once

#include <memory>
#include <netinet/tcp.h>
#include <sys/socket.h>

#include "zero/net/address.h"
#include "zero/base/noncopyable.h"

namespace zero {

// ============ Socket ============
class Socket : public std::enable_shared_from_this<Socket>, public Noncopyable {
public:
    using ptr = std::shared_ptr<Socket>;

    enum Type { TCP = SOCK_STREAM, UDP = SOCK_DGRAM };
    enum Family { IPv4 = AF_INET, IPv6 = AF_INET6, UNIX = AF_UNIX };

    // 工厂方法
    static Socket::ptr CreateTCP(Address::ptr addr);
    static Socket::ptr CreateUDP(Address::ptr addr);
    static Socket::ptr CreateTCPSocket(int family = AF_INET);
    static Socket::ptr CreateUDPSocket(int family = AF_INET);

    Socket(int family, int type, int protocol = 0);
    virtual ~Socket();

    // ---- Socket 操作 ----
    virtual bool init(int sock = -1);
    virtual bool bind(const Address::ptr addr);
    virtual bool listen(int backlog = SOMAXCONN);
    virtual Socket::ptr accept();
    virtual bool connect(const Address::ptr addr, uint64_t timeout_ms = -1);
    virtual bool close();
    bool isClosed() const { return is_closed_; }

    // ---- IO ----
    virtual ssize_t send(const void* buf, size_t len, int flags = 0);
    virtual ssize_t recv(void* buf, size_t len, int flags = 0);

    // ---- 选项 ----
    int64_t getSendTimeout();
    void    setSendTimeout(int64_t ms);
    int64_t getRecvTimeout();
    void    setRecvTimeout(int64_t ms);

    bool getOption(int level, int optname, void* result, size_t* len);
    bool setOption(int level, int optname, const void* val, size_t len);

    template<typename T>
    bool getOption(int level, int optname, T& result) {
        size_t len = sizeof(T);
        return getOption(level, optname, &result, &len);
    }

    template<typename T>
    bool setOption(int level, int optname, const T& value) {
        return setOption(level, optname, &value, sizeof(T));
    }

    void setTcpNoDelay(bool on);

    // ---- 属性 ----
    Address::ptr getRemoteAddress();
    Address::ptr getLocalAddress();
    int  getSocket()  const { return sock_; }
    int  getFamily()  const { return family_; }
    int  getType()    const { return type_; }
    bool isValid()    const;
    int  getError();

protected:
    int sock_ = -1;
    int family_;
    int type_;
    int protocol_;
    bool is_connected_ = false;
    bool is_closed_ = true;
    Address::ptr local_addr_;
    Address::ptr remote_addr_;
};

} // namespace zero
