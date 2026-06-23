#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ostream>

namespace zero {

// ============ Address ============
class Address {
public:
    using ptr = std::shared_ptr<Address>;

    virtual ~Address() = default;

    // 工厂方法
    static Address::ptr Create(const sockaddr* addr, socklen_t len);

    // DNS 解析
    static bool Lookup(std::vector<Address::ptr>& result, const std::string& host,
                       int family = AF_INET, int type = 0, int protocol = 0);
    static Address::ptr LookupAny(const std::string& host,
                                   int family = AF_INET, int type = 0, int protocol = 0);

    // 网卡地址
    static bool GetInterfaceAddresses(
        std::multimap<std::string, std::pair<Address::ptr, uint32_t>>& result,
        int family = AF_INET);
    static bool GetInterfaceAddresses(
        std::vector<std::pair<Address::ptr, uint32_t>>& result,
        const std::string& iface, int family = AF_INET);

    // 虚方法
    virtual int getFamily() const = 0;
    virtual const sockaddr* getAddr() const = 0;
    virtual sockaddr* getAddr() = 0;
    virtual socklen_t getAddrLen() const = 0;
    virtual std::ostream& insert(std::ostream& os) const = 0;

    std::string toString() const;

    bool operator<(const Address& rhs) const;
    bool operator==(const Address& rhs) const;
    bool operator!=(const Address& rhs) const;
};

// ============ IPv4Address ============
class IPv4Address : public Address {
public:
    using ptr = std::shared_ptr<IPv4Address>;

    IPv4Address(uint32_t addr = INADDR_ANY, uint16_t port = 0);
    IPv4Address(const sockaddr_in& addr);

    static IPv4Address::ptr Create(const char* addr, uint16_t port = 0);

    int getFamily() const override { return AF_INET; }
    const sockaddr* getAddr() const override { return (const sockaddr*)&addr_; }
    sockaddr* getAddr() override { return (sockaddr*)&addr_; }
    socklen_t getAddrLen() const override { return sizeof(addr_); }
    std::ostream& insert(std::ostream& os) const override;

    uint16_t getPort() const  { return ntohs(addr_.sin_port); }
    void     setPort(uint16_t v) { addr_.sin_port = htons(v); }
    uint32_t getIP() const    { return addr_.sin_addr.s_addr; }
    std::string getIPString() const;

    IPv4Address::ptr broadcastAddress(uint32_t prefix_len) const;
    IPv4Address::ptr networkAddress(uint32_t prefix_len) const;
    IPv4Address::ptr subnetMask(uint32_t prefix_len) const;

private:
    sockaddr_in addr_;
};

// ============ IPv6Address ============
class IPv6Address : public Address {
public:
    using ptr = std::shared_ptr<IPv6Address>;

    IPv6Address();
    IPv6Address(const sockaddr_in6& addr);
    IPv6Address(const uint8_t addr[16], uint16_t port = 0);

    static IPv6Address::ptr Create(const char* addr, uint16_t port = 0);

    int getFamily() const override { return AF_INET6; }
    const sockaddr* getAddr() const override { return (const sockaddr*)&addr_; }
    sockaddr* getAddr() override { return (sockaddr*)&addr_; }
    socklen_t getAddrLen() const override { return sizeof(addr_); }
    std::ostream& insert(std::ostream& os) const override;

    uint16_t getPort() const { return ntohs(addr_.sin6_port); }
    void     setPort(uint16_t v) { addr_.sin6_port = htons(v); }
    std::string getIPString() const;

private:
    sockaddr_in6 addr_;
};

// ============ UnixAddress ============
class UnixAddress : public Address {
public:
    using ptr = std::shared_ptr<UnixAddress>;

    UnixAddress();
    explicit UnixAddress(const std::string& path);

    int getFamily() const override { return AF_UNIX; }
    const sockaddr* getAddr() const override { return (const sockaddr*)&addr_; }
    sockaddr* getAddr() override { return (sockaddr*)&addr_; }
    socklen_t getAddrLen() const override { return addr_len_; }
    std::ostream& insert(std::ostream& os) const override;

    void setAddrLen(uint32_t v) { addr_len_ = v; }

private:
    sockaddr_un addr_;
    socklen_t addr_len_;
};

std::ostream& operator<<(std::ostream& os, const Address& addr);

} // namespace zero
