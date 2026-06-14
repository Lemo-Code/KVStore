#ifndef __SYLAR_SOCKET_H__
#define __SYLAR_SOCKET_H__
#include "address.h"
#include "noncopyable.h"
#include <openssl/err.h>
#include <openssl/ssl.h>

#include <memory>
#include <netinet/tcp.h>

namespace sylar
{

class Socket : public std::enable_shared_from_this<Socket>,Noncopyable
{
public:
    enum Type{
        TCP = SOCK_STREAM,
        UDP = SOCK_DGRAM,
    };

    enum Family{
        IPv4 = AF_INET,
        IPv6 = AF_INET6,
        UNIX = AF_UNIX,
    };

    typedef std::shared_ptr<Socket> ptr;
    typedef std::weak_ptr<Socket> weak_ptr;

    static Socket::ptr CreateTCP(sylar::Address::ptr address);
    static Socket::ptr CreateUDP(sylar::Address::ptr address);

    static Socket::ptr CreateTCPSocket();
    static Socket::ptr CreateUDPSocket();

    static Socket::ptr CreateTCPSocket6();
    static Socket::ptr CreateUDPSocket6();
    static Socket::ptr CreateUnixTCPSocket();
    static Socket::ptr CreateUnixUDPSocket();

    Socket(int family, int type, int protocol = 0);
    ~Socket();

    int64_t getSendTimeout();
    void setSendTimeout(int64_t v);

    int64_t getRecvTimeout();
    void setRecvTimeout(int64_t v);

    //get option
    bool getOption(int level, int option, void *result, size_t *len);
    template <class T>
    bool getOption(int level, int option, T &result){
        size_t length = sizeof(T);
        return getOption(level, option, &result, &length);
    }
    //setoption
    bool setOption(int level, int option, const void *result, size_t len);
    template <class T>
    bool setOption(int level, int option, const T &value){
        return setOption(level, option, &value, sizeof(T));
    }

    //accept
    virtual Socket::ptr accept();

    virtual bool init(int sock);
    virtual bool bind(const Address::ptr addr);
    virtual bool connect(const Address::ptr addr, uint64_t timeout_ms = -1);
    virtual bool listen(int backlog = SOMAXCONN);
    virtual bool close();
    virtual bool isClose()const;
    // tcp
    virtual int send(const void* buffers, size_t length, int flags = 0);
    virtual int send(const iovec* buffers, size_t length, int flags = 0);
    // udp
    virtual int sendTo(const void* buffers, size_t length, const Address::ptr to, int flags = 0);
    virtual int sendTo(const iovec* buffers, size_t length, const Address::ptr to, int flags = 0);
    // tcp
    virtual int recv(void *buffer, size_t length, int flags = 0);
    virtual int recv(iovec *buffer, size_t length, int flags = 0);
    // udp
    virtual int recvFrom(void* buffer, size_t length, Address::ptr from, int flags = 0);
    virtual int recvFrom(iovec* buffer, size_t length, Address::ptr from, int flags = 0);

    //socket 连接的远端的Address
    Address::ptr getRemoteAddress();
    //socket 本地的Address
    Address::ptr getLocalAddress();

    int getFamily() const {return family_;}
    int getType() const {return type_;}
    int getProtocol() const {return protocol_;}

    bool isConnected() const {return isConnected_;}
    bool isValid() const;
    int getError();

    virtual std::ostream& dump(std::ostream& os) const;
    int getSocket() const {return sock_;}

    //回调事件
    bool cancelRead();
    bool cancelWrite();
    bool cancelAccept();
    bool cancelAll();
private:
    void initSock();
    void newSock();
protected:
    int sock_;
    int family_;
    int type_;
    int protocol_;
    bool isConnected_;

    Address::ptr localAddress_;
    Address::ptr remoteAddress_;
};


class SSLSocket : public Socket {
public:
    typedef std::shared_ptr<SSLSocket> ptr;

    static SSLSocket::ptr CreateTCP(sylar::Address::ptr address);
    static SSLSocket::ptr CreateTCPSocket();
    static SSLSocket::ptr CreateTCPSocket6();

    SSLSocket(int family, int type, int protocol = 0);
    virtual Socket::ptr accept() override;
    bool bind(const Address::ptr addr) override;
    bool connect(const Address::ptr addr, uint64_t timeout_ms = -1) override;
    bool listen(int backlog = SOMAXCONN) override;
    bool close() override;
    int send(const void* buffer, size_t length, int flags = 0) override;
    int send(const iovec* buffers, size_t length, int flags = 0) override;
    int sendTo(const void* buffer, size_t length, const Address::ptr to, int flags = 0) override;
    int sendTo(const iovec* buffers, size_t length, const Address::ptr to, int flags = 0) override;
    int recv(void* buffer, size_t length, int flags = 0) override;
    int recv(iovec* buffers, size_t length, int flags = 0) override;
    int recvFrom(void* buffer, size_t length, Address::ptr from, int flags = 0) override;
    int recvFrom(iovec* buffers, size_t length, Address::ptr from, int flags = 0) override;

    bool loadCertificates(const std::string& cert_file, const std::string& key_file);
    std::ostream& dump(std::ostream& os) const override;
protected:
    virtual bool init(int sock) override;
private:
    std::shared_ptr<SSL_CTX> ctx_;
    std::shared_ptr<SSL> ssl_;
};


//重载字符流 让socket字符串可以用流的方式传入
std::ostream& operator<<(std::ostream& os, const Socket& sock);

}

#endif