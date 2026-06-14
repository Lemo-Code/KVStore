#include "sylar/log.h"
#include "sylar/endian.h"
#include "sylar/address.h"

#include <string.h>
#include <netdb.h>
#include <ifaddrs.h>

namespace sylar
{
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");
    //create Mask (bits is bits bit)            6  16-6   (littleEndian)
    //T is uint16_t and bits is 6, result like (... 1)   0011 1111 1111 
    template<class T>
    static T CreateMask(uint32_t bits){
        return (1 << (sizeof(T) * 8 - bits)) - 1;
    }

    //计算有bits流多少个1
    template<class T>
    static uint32_t CountBytes(T value){
        uint32_t result = 0;
        for(; value ;result++){//1111 1110   1111 1101 
            value &= value - 1;
        }
        return result;
    }
    /**
     * IPAddress
     */
    //wrap manyType socket
    //return [IPv4  IPv6  Unknown]
    Address::ptr Address::Create(const sockaddr* addr,socklen_t addrLen)
    {
        if(addr == nullptr){
            return nullptr;
        }

        Address::ptr result;
        switch (addr->sa_family)
        {
        case AF_INET:
            result.reset(new IPv4Address(*(const sockaddr_in*)addr));
            break;
        case AF_INET6:
            result.reset(new IPv6Address(*(const sockaddr_in6*)addr));
            break;
        default:
            result.reset(new UnknownAddress(*addr));
            break;
        }
        return result;
    }
    // parse domain address -> create Address object
    bool Address::Lookup(std::vector<Address::ptr> &result, const std::string &host,
                int family , int type, int protocol){
        /* int getaddrinfo(const char *node, const char *service,
           const struct addrinfo *hints,
           struct addrinfo **res); */
        addrinfo hints,*results,*next;
        hints.ai_flags = 0;
        hints.ai_family = family;
        hints.ai_socktype = type;
        hints.ai_protocol = protocol;
        hints.ai_addrlen = 0;
        hints.ai_canonname = NULL;
        hints.ai_addr = NULL;
        hints.ai_next = NULL;

        std::string node;
        const char* service = NULL;

        //处理ipv6 的port
        if(!host.empty() && host[0] == '['){
            const char* endipv6 = (const char*)memchr(host.c_str()+1,']',host.size() - 1);
            if(endipv6){
                if(*(endipv6 + 1) == ':'){
                    service = endipv6 + 2;
                }
                node = host.substr(1,endipv6 - host.c_str() - 1);
            }
        }

        //处理ipv4的port
        if(node.empty()){
            service = (const char*)memchr(host.c_str(),':',host.size());
            if(service){
                if(!memchr(service + 1, ':', host.c_str() + host.size() - service -1)){
                    node = host.substr(0,service - host.c_str());
                    ++service;
                }
            }
        }

        if(node.empty()){
            node = host;
        }
        int error = getaddrinfo(node.c_str(),service,&hints,&results);
        if(error){
            SYLAR_LOG_ERROR(g_logger) << "Address::Lookup getaddress(" 
                << host <<", "<< family <<", " <<type <<") err="
                << error <<" errstr=" <<strerror(errno);
                return false;
        }

        next = results;
        while(next){
            result.push_back(Address::Create(next->ai_addr,(socklen_t)next->ai_addrlen));
            next = next->ai_next;
        }

        freeaddrinfo(results);
        return !result.empty();
    }
    //get AnyAddress::ptr from result
    Address::ptr Address::LookupAny(const std::string& host,int family, 
                            int type ,int protocol){
            std::vector<Address::ptr> result;
            if(Lookup(result,host,family,type,protocol)){
                return result[0];
            }
            return nullptr;
        }
    //get IPAddress::ptr from result
    IPAddress::ptr Address::LookupAnyIPAddress(const std::string& host,int family ,
                            int type,int protocol){
            std::vector<Address::ptr> result;
            if (Lookup(result, host, family, type, protocol)){
                for(auto& i: result){
                    IPAddress::ptr v = std::dynamic_pointer_cast<IPAddress>(i);
                    if(v){
                        return v;
                    }
                }
            }
            return nullptr;
        }

    // not test
    std::shared_ptr<UnixAddress> Address::LookupAnyUnixAddress(const std::string& host,
                    int family , int type , int protocol ){
            std::vector<Address::ptr> result;
            if (Lookup(result, host, family, type, protocol)){
                    for(auto& i: result){
                        UnixAddress::ptr v = std::dynamic_pointer_cast<UnixAddress>(i);
                        if(v){
                            return v;
                        }
                    }
                }
            return nullptr;
        }
//34:38
    bool Address::GetInterFaceAddress(std::multimap<std::string,
            std::pair<Address::ptr,uint32_t> >&result,
            int family){
                struct ifaddrs *next,*results;
                if(getifaddrs(&results) != 0){
                    SYLAR_LOG_ERROR(g_logger) << "Address::GetInterFaceAddress getifaddrs "
                        << " err=" << errno << " errstr=" << strerror(errno);
                    return false;
                }

            try{
                for(next = results ; next ; next = next->ifa_next){
                    Address::ptr addr;
                    uint32_t prefix_length = ~0u;
                    if(family != AF_UNSPEC && family != next->ifa_addr->sa_family){
                        continue;
                    }
                    switch(next->ifa_addr->sa_family)
                    {
                        case AF_INET:
                            {
                                addr = Create(next->ifa_addr,sizeof(sockaddr_in));
                                //不懂
                                uint32_t netmask = ((sockaddr_in*)next->ifa_netmask)->sin_addr.s_addr;
                                prefix_length = CountBytes(netmask);
                            }    
                            break;
                        case AF_INET6:
                            {
                                addr = Create(next->ifa_addr,sizeof(sockaddr_in6));
                                //不懂
                                in6_addr& netmask = ((sockaddr_in6*)next->ifa_netmask)->sin6_addr;
                                prefix_length = 0;
                                for(int i=0;i<16;i++){
                                    prefix_length += CountBytes(netmask.s6_addr[i]);
                                }
                            }
                            break;
                        default:
                            break;
                    }

                    if(addr){
                        // SYLAR_LOG_INFO(g_logger) << "addr:" << *addr;
                        result.insert(std::make_pair(next->ifa_name,
                                std::make_pair(addr,prefix_length)));
                    }
                }
            } catch(...) {
                SYLAR_LOG_ERROR(g_logger) << "Address::GetInterFaceAddress exception";
                freeifaddrs(results);
                return false;
            }
            freeifaddrs(results);
                return !result.empty();
        }
    //iface1应该使用网卡名称的
    bool Address::GetInterFaceAddress(
            std::vector<std::pair<Address::ptr,uint32_t> >&result,
            const std::string& iface,int family){
                // std::string iface = "ens33";
                //没懂
                // SYLAR_LOG_FATAL(g_logger) << iface;   0.0.0.0
                if(iface.empty() || iface == "*"){
                    if(family == AF_INET || family == AF_UNSPEC){
                        result.push_back(std::make_pair(Address::ptr(new IPv4Address()) ,0u));
                    }
                    if(family == AF_INET6 || family == AF_UNSPEC){
                        result.push_back(std::make_pair(Address::ptr(new IPv6Address()),0u));
                    }
                    return true;
                }

                std::multimap<std::string,
                        std::pair<Address::ptr,uint32_t>> results;
                //作用是获取所有的网卡 存储到result中
                if(!GetInterFaceAddress(results,family)){
                    // SYLAR_LOG_FATAL(g_logger) << "getInterFaceAddress false"; //没走这里
                    return false;
                }

                auto its = results.equal_range(iface);
                for(; its.first != its.second;its.first++){
                    // SYLAR_LOG_INFO(g_logger) << "xxx";
                    result.push_back(its.first->second);
                }

                //比较网卡名称
                // for(auto& i : results){
                //     if(i.second.first->toString() == iface){
                //         result.push_back(i.second);
                //         SYLAR_LOG_INFO(g_logger) << "other myadd";
                //     }
                // }
                return !result.empty();
            }

    //funny Design 
    int Address::getFamily() const
    {
        return getAddr()->sa_family;
    }
    
    std::string Address::toString()const
    {
        std::stringstream ss;
        insert(ss);
        return ss.str();   
    }

    bool Address::operator<(const Address& rhs) const
    {
        socklen_t minlen = std::min(getAddrLen(),rhs.getAddrLen());
        int res = memcmp(getAddr(),rhs.getAddr(),minlen);
        if(res < 0){
            return true;
        } else if(res > 0){
            return false;
        } else if(getAddrLen()< rhs.getAddrLen()){
            return true;
        }
        return false;
    }
    
    bool Address::operator==(const Address& rhs) const
    {
        return getAddrLen() == rhs.getAddrLen()
                && memcmp(getAddr(),rhs.getAddr(),getAddrLen()) == 0;
    }
    
    bool Address::operator!=(const Address& rhs) const
    {
        return !(*this == rhs);
    }

    /**
     * IPv4Address
     */
    //const char* ->  s_addr
    IPv4Address::ptr IPv4Address::Create(const char* address,uint16_t port)
    {
        IPv4Address::ptr rt(new IPv4Address);
        rt->addr_.sin_port = byteswapOnLittleEndian(port);
        rt->addr_.sin_family = AF_INET;
        int res = inet_pton(AF_INET,address,&rt->addr_.sin_addr.s_addr);
        if(res <= 0){
            SYLAR_LOG_ERROR(g_logger) << "IPv4Address::Create(" <<address <<","
                    << port <<") rt=" << res <<" errno=" << errno
                    << strerror(errno);
            return nullptr;
        }
        return rt;
    }
    IPv4Address::IPv4Address(uint32_t address, uint16_t port)
    {
        memset(&addr_,0,sizeof(addr_));
        addr_.sin_family = AF_INET;
        addr_.sin_port = byteswapOnLittleEndian(port);
        addr_.sin_addr.s_addr = byteswapOnLittleEndian(address);
    }

    IPv4Address::IPv4Address(const sockaddr_in& addr)
    {   
        addr_ = addr;
    }
    //get addr:port  eg: x.x.x.x:p
    std::ostream& IPv4Address::insert(std::ostream &os)const
    {
        //addr_.sin_addr.s_addr-struted is bigEndian
        //this step swap to (littleEndian) which is convenient to be recognized by host
        uint32_t addr = byteswapOnLittleEndian(addr_.sin_addr.s_addr);
        os << ((addr >> 24) & 0xff)<<"."
            << ((addr >> 16) & 0xff)<<"."
            << ((addr >> 8) & 0xff)<<"."
            << (addr & 0xff);
        os << ":"<<byteswapOnLittleEndian(addr_.sin_port);
        return os;
    }
    std::ostream& IPv4Address::insertAddr(std::ostream &os)const{
        uint32_t addr = byteswapOnLittleEndian(addr_.sin_addr.s_addr);
        os << ((addr >> 24) & 0xff)<<"."
            << ((addr >> 16) & 0xff)<<"."
            << ((addr >> 8) & 0xff)<<"."
            << (addr & 0xff);
        return os;
    }
    std::string IPv4Address::AddrString()const{
        std::stringstream os;
        insertAddr(os);
        return os.str();
    }

    // get sockaddr
    const sockaddr *IPv4Address::getAddr() const
    {
        return (sockaddr*)&addr_;
    }
    sockaddr *IPv4Address::getAddr()
    {
        return (sockaddr*)&addr_;
    }
    // get sockaddrLen
    socklen_t IPv4Address::getAddrLen() const
    {
        return sizeof(addr_);
    }
   
//还没处理完
    //input a constchar* Address(numberic address(not demain address))  get IPAddress
    //accept numeric ip
    IPAddress::ptr IPAddress::Create(const char* address,uint16_t port)
    {
        //results is a link of Address ,because address of a host owns many address(ipv4 ipv6 ...)
        addrinfo hints,*results; 
        memset(&hints,0,sizeof(hints));

        hints.ai_flags = AI_NUMERICHOST; //numberic ip
        //hints.ai_flags = AI_CANONNAME; //numberic ip
        hints.ai_family = AF_UNSPEC;    //not assign specific ipAF（accept AllAF）

        int error = getaddrinfo(address,NULL,&hints,&results);
        if(error){
            SYLAR_LOG_ERROR(g_logger)<< "IPAddress::Create(" <<address
                << ", " << port << ") error" << error 
                <<"error=" << errno <<" errstr="<<
                strerror(errno);
            return nullptr;
        }
        
        try{
            IPAddress::ptr result = std::dynamic_pointer_cast<IPAddress>(
                Address::Create(results->ai_addr,(socklen_t)results->ai_addrlen));
            if(result){
                result->setPort(port);
            }
            freeaddrinfo(results);
            return result;
        } catch(...){
            freeaddrinfo(results);
            return nullptr;
        }
    }

    // get the broadcastAddress of the original sockaddr
    // 4    192.168.1.6  -> 192.255.255.255
    IPAddress::ptr IPv4Address::broadcastAddress(uint32_t prefix_len)
    {
        if(prefix_len > 32){
            return nullptr;
        }

        sockaddr_in baddr(addr_);
        baddr.sin_addr.s_addr |= byteswapOnLittleEndian(
                CreateMask<uint32_t>(prefix_len));//prefix个0   32-prefix个1
        return IPv4Address::ptr(new IPv4Address(baddr));
    }

    //get the networkAddress of the original sockaddr
    // 4    192.168.1.6  -> 255.0.0.0
    IPAddress::ptr IPv4Address::networkAddress(uint32_t prefix_len)
    {
        if(prefix_len > 32){
            return nullptr;
        }

        sockaddr_in baddr(addr_);
        baddr.sin_addr.s_addr &= ~byteswapOnLittleEndian(
                CreateMask<uint32_t>(prefix_len));//prefix个1  32-pre个0
        return IPv4Address::ptr(new IPv4Address(baddr));
    }
    
    //get the subnetMask of the original sockaddr
    IPAddress::ptr IPv4Address::subnetMask(uint32_t prefix_len)
    {
        sockaddr_in baddr;
        memset(&baddr,0,sizeof(baddr));
        baddr.sin_family = AF_INET;
        baddr.sin_addr.s_addr = ~byteswapOnLittleEndian(
            CreateMask<uint32_t>(prefix_len));//prefix个1  32-pre个0
        return IPv4Address::ptr(new IPv4Address(baddr));
    }

    void IPv4Address::setPort(uint16_t v)
    {
        addr_.sin_port = byteswapOnLittleEndian(v);
    }
    // returned is littleEndian Port
    uint16_t IPv4Address::getPort() const
    {
        return byteswapOnLittleEndian(addr_.sin_port);
    }

    /**
     * IPv6Address
     */
    IPv6Address::IPv6Address()
    {
        memset(&addr_,0,sizeof(addr_));
        addr_.sin6_family = AF_INET6;
    }
    //const char* -> s6_addr
    IPv6Address::ptr IPv6Address::Create(const char* address ,uint16_t port)
    { 
        //std::shared_ptr<IPv6Address> baddr = std::make_shared<IPv6Address>();
        std::shared_ptr<IPv6Address> rt(new IPv6Address);
        rt->addr_.sin6_port = byteswapOnLittleEndian(port);
        int res = inet_pton(AF_INET6,address,&rt->addr_.sin6_addr.s6_addr);
        if(res <= 1){
            SYLAR_LOG_ERROR(g_logger) << "IPv6Address::Create(" <<address <<","
                    << port <<") rt=" << res <<" errno=" << errno
                    << strerror(errno);
            return nullptr;
        }
        return rt;
    }
    IPv6Address::IPv6Address(const uint8_t address[16],uint16_t port)
    {
        memset(&addr_,0,sizeof(addr_));
        addr_.sin6_family = AF_INET6;
        addr_.sin6_port = byteswapOnLittleEndian(port);
        memcpy(&addr_.sin6_addr.s6_addr,address,16);
    }
    IPv6Address::IPv6Address(const sockaddr_in6& addr)
    {
        addr_ = addr;
    }
    //get [addr:port]      16*8
    std::ostream &IPv6Address::insert(std::ostream &os)const
    {
        os << "[";
        uint16_t* addr = (uint16_t*)addr_.sin6_addr.s6_addr;
        bool used_zero = false;
        //continuous zero occurs, the first continuous zero occured will be replaced by :
        for(size_t i = 0;i < 8;i++){
            if(addr[i] == 0 && !used_zero){
                continue;
            }
            if(i && addr[i-1] == 0 && !used_zero){
                os << ":";
                used_zero = true;
            }
            if(i){
                os<<":";
            }
            os<< std::hex << (int)byteswapOnLittleEndian(addr[i]) << std::dec;
        }

        if(!used_zero && addr[7] == 0){
            os<<"::";
        }

        os << "]:" <<byteswapOnLittleEndian(addr_.sin6_port);
        return os;
    }

    std::string IPv6Address::AddrString()const{
        std::stringstream os;
        insertAddr(os);
        return os.str();
    }

    std::ostream& IPv6Address::insertAddr(std::ostream& os)const {
        os << "[";
        uint16_t* addr = (uint16_t*)addr_.sin6_addr.s6_addr;
        bool used_zero = false;
        //continuous zero occurs, the first continuous zero occured will be replaced by :
        for(size_t i = 0;i < 8;i++){
            if(addr[i] == 0 && !used_zero){
                continue;
            }
            if(i && addr[i-1] == 0 && !used_zero){
                os << ":";
                used_zero = true;
            }
            if(i){
                os<<":";
            }
            os<< std::hex << (int)byteswapOnLittleEndian(addr[i]) << std::dec;
        }

        if(!used_zero && addr[7] == 0){
            os<<"::";
        }

        os << "]";
        return os;
    }

    const sockaddr *IPv6Address::getAddr() const
    {
        return (sockaddr*)&addr_;
    }

    sockaddr *IPv6Address::getAddr()
    {
        return (sockaddr*)&addr_;
    }
    socklen_t IPv6Address::getAddrLen() const
    {
        return sizeof(addr_);
    }

    //get IPv6Address with the broadcastSockAddress of srcSockAddress 
    //prefix_len/8 makeMask And from prefix_len/8 + 1 ,allbits is 1
    IPAddress::ptr IPv6Address::broadcastAddress(uint32_t prefix_len)
    {//[fe80::5054:ff:fe7c:e28d]
        sockaddr_in6 baddr(addr_);
        baddr.sin6_addr.s6_addr[prefix_len/8] |= CreateMask<uint8_t>(prefix_len%8);
        for (size_t i = prefix_len / 8 + 1; i < 16; i++){
            baddr.sin6_addr.s6_addr[i] |= 0xff;
        }
        return IPv6Address::ptr(new IPv6Address(baddr));
    }
    //get IPv6Address with the networkSockAddress of srcSockAddress 
    IPAddress::ptr IPv6Address::networkAddress(uint32_t prefix_len)
    {
        //[fe80::5054:ff:fe7c:e28d]
        sockaddr_in6 baddr(addr_);
        baddr.sin6_addr.s6_addr[prefix_len/8] &= ~CreateMask<uint8_t>(prefix_len%8);
        for (size_t i = prefix_len / 8 + 1; i < 16; i++){
            baddr.sin6_addr.s6_addr[i] |= 0x00;
        }
        return IPv6Address::ptr(new IPv6Address(baddr));
    }
    //get IPv6Address of subnetMask 
    IPAddress::ptr IPv6Address::subnetMask(uint32_t prefix_len)
    {
        sockaddr_in6 baddr;
        memset(&baddr,0,sizeof(baddr));
        baddr.sin6_family = AF_INET6;
        for(size_t i = 0;i < prefix_len/8;i++){
            baddr.sin6_addr.s6_addr[i] = 0xFF;
        }
        baddr.sin6_addr.s6_addr[prefix_len/8] = 
                ~CreateMask<uint8_t>(prefix_len%8);
        return IPv6Address::ptr(new IPv6Address(baddr));
    }

    //addr_.sin6_port is BigEndian
    void IPv6Address::setPort(uint16_t v)
    {
        addr_.sin6_port = byteswapOnLittleEndian(v);
    }
    //returned is LittleEndian port
    uint16_t IPv6Address::getPort() const
    {
        return byteswapOnLittleEndian(addr_.sin6_port);
    }

    /**
     * UnixAddress
     */
    static const size_t MAX_PATH_LEN = sizeof(((sockaddr_un*)0)->sun_path) - 1; //减去"/0"

    UnixAddress::UnixAddress()
    {
        memset(&addr_,0,sizeof(addr_));
        addr_.sun_family = AF_UNIX;
        length_ = offsetof(sockaddr_un,sun_path) + MAX_PATH_LEN;
    }
    UnixAddress::UnixAddress(const std::string &path)
    {
        memset(&addr_,0,sizeof(addr_));
        addr_.sun_family = AF_UNIX;
        length_ = path.size() + 1; //+"\0"

        if(!path.empty() && path[0] == '\0'){
            --length_;
        }

        if(length_ > sizeof(addr_.sun_path)){
            throw std::logic_error("unixAddr path too long");
        }
        memcpy(&addr_.sun_path,path.c_str(),length_);
        length_ += offsetof(sockaddr_un,sun_path);
    }
    const sockaddr *UnixAddress::getAddr() const
    {
        return (sockaddr*)&addr_;
    }

    sockaddr *UnixAddress::getAddr()
    {
        return (sockaddr*)&addr_;
    }
    socklen_t UnixAddress::getAddrLen() const
    {
        return length_;
    }
    std::ostream &UnixAddress::insert(std::ostream &os)const
    {
        //handle unixlength's head is "/0"
        if(length_ > offsetof(sockaddr_un , sun_path)
            && addr_.sun_path[0] == '\0'){
                return os << "\\0" << std::string(addr_.sun_path + 1 
                        ,length_ - offsetof(sockaddr_un,sun_path) - 1);
            }
         return os << addr_.sun_path;
    }
    void UnixAddress::setAddrLen(uint32_t v) {
        length_ = v;
    }
    
    /**
     * UnknownAddress
     */
    UnknownAddress::UnknownAddress(int family)
    {
        memset(&addr_,0,sizeof(addr_));
        addr_.sa_family = family;
    }

    UnknownAddress::UnknownAddress(const sockaddr& addr)
    {
        addr_ = addr;
    }

    const sockaddr* UnknownAddress::getAddr() const
    {
        return &addr_;
    }
    sockaddr* UnknownAddress::getAddr() 
    {
        return &addr_;
    }
    
    socklen_t UnknownAddress::getAddrLen() const
    {
        return sizeof(addr_);
    }

    std::ostream& UnknownAddress::insert(std::ostream& os)const
    {
        os << "UnknownAddress family=" << addr_.sa_family <<"]";
        return os;
    }


    std::ostream& operator<<(std::ostream& os,const Address& addr){
        addr.insert(os);
        return os;
    }

}//sylar