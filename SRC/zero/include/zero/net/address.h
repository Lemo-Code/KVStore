// zero Address — IPv4/IPv6/Unix socket address abstraction
//
// Polymorphic address hierarchy wrapping sockaddr_storage:
//   - IPv4Address: AF_INET, holds sockaddr_in
//   - IPv6Address: AF_INET6, holds sockaddr_in6
//   - UnixAddress: AF_UNIX, holds sockaddr_un
//
// Factory methods for DNS resolution and raw sockaddr construction.
// All addresses are immutable after construction (except port on IP
// addresses which can be updated via setPort).
#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <string>
#include <memory>
#include <vector>

namespace zero {

// ============================================================
// Address — abstract base class
// ============================================================

class Address {
public:
    enum class Type { IPv4, IPv6, Unix };

    virtual ~Address() = default;

    virtual Type type() const noexcept = 0;
    virtual const struct sockaddr* addr() const = 0;
    virtual socklen_t addr_len() const = 0;
    virtual std::string to_string() const = 0;

    // Convenience
    int family() const noexcept { return addr()->sa_family; }

    // Factory: create from a raw sockaddr (copies the data)
    static std::shared_ptr<Address> create(const struct sockaddr* addr,
                                             socklen_t len);

    // Factory: DNS-resolve host to a list of addresses.
    // If port != 0, the port is set on all resolved addresses.
    // Returns an empty vector on failure.
    static std::vector<std::shared_ptr<Address>> lookup_all(
        const std::string& host, int port = 0, int family = AF_UNSPEC);

    // Factory: resolve host and return the first matching address.
    // Convenience wrapper around lookup_all.
    static std::shared_ptr<Address> lookup(
        const std::string& host, int port = 0, int family = AF_UNSPEC);

    // Factory: create from "host:port" or "ip:port" string.
    // Examples: "127.0.0.1:8080", "::1:9090", "/tmp/unix.sock"
    static std::shared_ptr<Address> parse(const std::string& addr_str);
};

// ============================================================
// IPv4Address
// ============================================================

class IPv4Address : public Address {
public:
    // Construct from sockaddr_in (copies)
    explicit IPv4Address(const struct sockaddr_in& addr);

    // Construct from dotted-decimal IP and port
    IPv4Address(const std::string& ip, uint16_t port);

    // Construct for "any" address on the given port (0.0.0.0:port)
    explicit IPv4Address(uint16_t port = 0);

    Type type() const noexcept override { return Type::IPv4; }

    const struct sockaddr* addr() const override {
        return reinterpret_cast<const struct sockaddr*>(&addr_);
    }
    socklen_t addr_len() const override { return sizeof(addr_); }

    std::string to_string() const override;

    // Accessors
    std::string ip() const;
    uint16_t port() const;
    void set_port(uint16_t port);

    // Broadcast address for this subnet
    static std::shared_ptr<IPv4Address> broadcast(
        const std::string& ip, uint16_t port);

private:
    struct sockaddr_in addr_;
};

// ============================================================
// IPv6Address
// ============================================================

class IPv6Address : public Address {
public:
    explicit IPv6Address(const struct sockaddr_in6& addr);
    IPv6Address(const std::string& ip, uint16_t port);
    explicit IPv6Address(uint16_t port = 0);  // [::]:port

    Type type() const noexcept override { return Type::IPv6; }

    const struct sockaddr* addr() const override {
        return reinterpret_cast<const struct sockaddr*>(&addr_);
    }
    socklen_t addr_len() const override { return sizeof(addr_); }

    std::string to_string() const override;

    std::string ip() const;
    uint16_t port() const;
    void set_port(uint16_t port);

    // Scope ID (interface index for link-local addresses)
    uint32_t scope_id() const;
    void set_scope_id(uint32_t id);

private:
    struct sockaddr_in6 addr_;
};

// ============================================================
// UnixAddress
// ============================================================

class UnixAddress : public Address {
public:
    explicit UnixAddress(const std::string& path);
    // Abstract socket (Linux: path starts with '\0')
    UnixAddress(const std::string& path, bool abstract);

    Type type() const noexcept override { return Type::Unix; }

    const struct sockaddr* addr() const override {
        return reinterpret_cast<const struct sockaddr*>(&addr_);
    }
    socklen_t addr_len() const override;

    std::string to_string() const override;

    std::string path() const;
    bool is_abstract() const noexcept;

private:
    struct sockaddr_un addr_;
    bool abstract_;
    std::string path_;
};

} // namespace zero
