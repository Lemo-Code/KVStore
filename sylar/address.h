#ifndef __SYLAR_ADDRESS_H__
#define __SYLAR_ADDRESS_H__
/*
    模块设计方案

   [UnixAddress]
        |                 -------[IPv4Address]          
    -----------           |
    | Address |  ---- IPAddress
    -----------           |
        |                  -------[IPv6Address]
    [Unknown]
 */
#include <memory>
#include <vector>
#include <map>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h> //sockaddr_in
#include <sys/un.h>     //sockaddr_un
#include <sstream>      //stringstream
#include <arpa/inet.h>  //inet_pton
namespace sylar
{
    class IPAddress;
    class UnixAddress;
    /*
     * Address
     */
    class Address
    {
        public:
        typedef std::shared_ptr<Address> ptr;
        virtual ~Address() {}
        
        //ip address -> address
        static Address::ptr Create(const sockaddr* addr,socklen_t addrLen);
        //domain address 
        //AF_UNSPEC 任意类型
        static bool Lookup(std::vector<Address::ptr>& result,const std::string& host,
                    int family = AF_INET, int type = 0,int protocol = 0);
        static Address::ptr LookupAny(const std::string& host,
                    int family = AF_INET, int type = 0,int protocol = 0);
        static std::shared_ptr<IPAddress> LookupAnyIPAddress(const std::string& host,
                    int family = AF_INET, int type = 0,int protocol = 0);
        static std::shared_ptr<UnixAddress> LookupAnyUnixAddress(const std::string& host,
                    int family = AF_UNIX,int type = 0, int protocol = 0);
        //常用于服务器的socket xxx怎么使用？
        static bool GetInterFaceAddress(std::multimap<std::string,
                            std::pair<Address::ptr,uint32_t> >&result,
                            int family = AF_UNSPEC);
        static bool GetInterFaceAddress(
                            std::vector<std::pair<Address::ptr,uint32_t> >&result,
                            const std::string& iface,int family = AF_UNSPEC);
        int getFamily() const;

        virtual sockaddr* getAddr() = 0;
        virtual const sockaddr* getAddr() const = 0;
        virtual socklen_t getAddrLen() const = 0;
        virtual std::ostream& insert(std::ostream& os) const = 0;
        std::string toString()const;

        bool operator<(const Address& rhs) const;
        bool operator==(const Address& rhs) const;
        bool operator!=(const Address& rhs) const;
    };

    /*
     * IPAddress
     */
    class IPAddress:public Address
    {
        public:
        typedef std::shared_ptr<IPAddress> ptr;
         //const char*  ->  address
        static IPAddress::ptr Create(const char* address,uint16_t port = 0);
        virtual IPAddress::ptr broadcastAddress(uint32_t prefix_len) = 0;
        virtual IPAddress::ptr networkAddress(uint32_t prefix_len) = 0;
        virtual IPAddress::ptr subnetMask(uint32_t prefix_len) = 0;
        virtual std::ostream& insertAddr(std::ostream &os)const = 0;
        virtual std::string AddrString()const = 0;
        virtual uint16_t getPort() const = 0;
        virtual void setPort(uint16_t v) = 0;
    };

    /*
     * IPv4Address
     */
    class IPv4Address:public IPAddress
    {
        public:
        typedef std::shared_ptr<IPv4Address> ptr;

        static IPv4Address::ptr Create(const char* address,uint16_t port = 0);
        //INADDR_ANY 
        IPv4Address(uint32_t address = INADDR_ANY, uint16_t port = 0);
        IPv4Address(const sockaddr_in& addr);
        std::ostream& insert(std::ostream& os)const override;
        std::ostream& insertAddr(std::ostream &os)const override;
        std::string AddrString()const override;
        const sockaddr* getAddr() const override;
        sockaddr* getAddr() override;
        socklen_t getAddrLen() const override;

        IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;
        IPAddress::ptr networkAddress(uint32_t prefix_len) override;
        IPAddress::ptr subnetMask(uint32_t prefix_len) override;

        void setPort(uint16_t v)override;
        uint16_t getPort()const override;
        private:
        sockaddr_in addr_;
    };

    /*
     * IPv6Address
     */
    class IPv6Address:public IPAddress
    {
        public:
        typedef std::shared_ptr<IPv6Address> ptr;
        static IPv6Address::ptr Create(const char* address ,uint16_t port = 0);
        IPv6Address();
        IPv6Address(const sockaddr_in6& addr);
        IPv6Address(const uint8_t address[16],uint16_t port = 0);
        std::ostream& insert(std::ostream& os)const override;
        std::ostream& insertAddr(std::ostream& os)const override;
        std::string AddrString()const override;
        const sockaddr* getAddr() const override;
        sockaddr* getAddr() override;
        socklen_t getAddrLen() const override;

        IPAddress::ptr broadcastAddress(uint32_t prefix_len) override;
        IPAddress::ptr networkAddress(uint32_t prefix_len) override;
        IPAddress::ptr subnetMask(uint32_t prefix_len)override;

        void setPort(uint16_t v) override;
        uint16_t getPort()const override;
        private:
        sockaddr_in6 addr_;
    };

    /*
     * UnixAddress
     */
    class UnixAddress:public Address
    {
        public:
        typedef std::shared_ptr<UnixAddress> ptr;
        UnixAddress(const std::string& path);
        UnixAddress();
        const sockaddr* getAddr() const override;
        sockaddr* getAddr() override;
        socklen_t getAddrLen() const override;
        std::ostream& insert(std::ostream& os) const override;
        void setAddrLen(uint32_t v);
        private:
        sockaddr_un addr_;
        socklen_t length_;
    };

    /*
     *  UnknownAddress
     */
    class UnknownAddress:public Address
    {
        public:
        typedef std::shared_ptr<UnknownAddress> ptr;
        UnknownAddress(int family);
        UnknownAddress(const sockaddr& addr);
        const sockaddr* getAddr() const override;
        sockaddr* getAddr() override;
        socklen_t getAddrLen() const override;
        std::ostream& insert(std::ostream& os) const override;
        private:
        sockaddr addr_;
    };

    std::ostream& operator<<(std::ostream& os,const Address& addr);

}//sylar

#endif