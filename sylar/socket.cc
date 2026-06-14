
#include "sylar/log.h"
#include "sylar/iomanager.h"
#include "sylar/macro.h"
#include "sylar/fdmanager.h"
#include "sylar/hook.h"
#include "sylar/socket.h"

#include <netinet/tcp.h>

namespace sylar
{
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    Socket::ptr Socket::CreateTCP(sylar::Address::ptr address)
    {
        Socket::ptr sock(new Socket(address->getFamily(),TCP,0));
        return sock;
    }
    Socket::ptr Socket::CreateUDP(sylar::Address::ptr address)
    {
        Socket::ptr sock(new Socket(address->getFamily(),UDP,0));
        sock->newSock();
        sock->isConnected_ = true;
        return sock;
    }

    /**
     * IPv4
     */
    Socket::ptr Socket::CreateTCPSocket()
    {
        Socket::ptr sock(new Socket(IPv4,TCP,0));
        return sock;
    }
    Socket::ptr Socket::CreateUDPSocket()
    {
        Socket::ptr sock(new Socket(IPv4,UDP,0));
        sock->newSock();
        sock->isConnected_ = true;
        return sock;
    }
    
    Socket::ptr Socket::CreateTCPSocket6()
    {
        Socket::ptr sock(new Socket(IPv6,TCP,0));
        return sock;
    }
    Socket::ptr Socket::CreateUDPSocket6()
    {
        Socket::ptr sock(new Socket(IPv6,UDP,0));
        sock->newSock();
        sock->isConnected_ = true;
        return sock;
    }


    Socket::ptr Socket::CreateUnixTCPSocket()
    {
        Socket::ptr sock(new Socket(UNIX,TCP,0));
        return sock;
    }
    Socket::ptr Socket::CreateUnixUDPSocket()
    {
        Socket::ptr sock(new Socket(UNIX,UDP,0));
        return sock;
    }
    // 构造函数中并没有进行 fd的构造 fd的构造是在（init函数中）
    Socket::Socket(int family, int type, int protocol) 
        : sock_(-1)
        , family_(family)
        , type_(type)
        , protocol_(protocol)
        , isConnected_(false){

    }

    Socket::~Socket() 
    {
        close();
    }

    int64_t Socket::getSendTimeout() 
    {
        //从 fdManager::vec中取一个fd(也可以说把fd存入vec)
        FdCtx::ptr ctx = FdMgr::GetInstance()->get(sock_);
        if(ctx){
            return ctx->getTimeout(SO_SNDTIMEO);
        }
        return -1;
    }

    void Socket::setSendTimeout(int64_t v) 
    {
        // s   us     v->ms
        struct timeval tv{int(v / 1000),int(v % 1000 * 1000)};
        setOption(SOL_SOCKET,SO_SNDTIMEO,tv);
    }

    // ---- recv
    // false -1
    int64_t Socket::getRecvTimeout() 
    {
        FdCtx::ptr ctx = FdMgr::GetInstance()->get(sock_);
        if(ctx){
            return ctx->getTimeout(SO_RCVTIMEO);
        }
        return -1;
    }

    void Socket::setRecvTimeout(int64_t v) 
    {
        struct timeval tv{int(v / 1000),int(v % 1000 * 1000)};    
        setOption(SOL_SOCKET,SO_RCVTIMEO,tv);
    }

    bool Socket::getOption(int level, int option, void *result, size_t *len) 
    {
        int rt = getsockopt(sock_,level,option,result,(socklen_t*)len);  
        if(rt){
            SYLAR_LOG_DEBUG(g_logger) << "getOption sock=" << sock_ 
                    << " level=" << level << " option=" << option
                    << " errno=" << errno << " errstr=" <<strerror(errno);
            return false;
        }  
        return true;
    }

    bool Socket::setOption(int level, int option, const void *result, size_t len) 
    {
        int rt = setsockopt(sock_,level,option,result,(socklen_t)len);
        if(rt){
            SYLAR_LOG_WARN(g_logger) << "setOption sock=" << sock_ 
                    << " level=" << level << " option=" << option
                    << " errno=" << errno << " errstr=" <<strerror(errno);
            return false;
        }
        return true;
    }

    Socket::ptr Socket::accept() 
    {
        // SYLAR_LOG_DEBUG(g_logger) << "Socket::accept()";
        // 构造函数中并没有进行 fd的构造 fd的构造是在（init函数中）
        Socket::ptr sock(new Socket(family_,type_,protocol_));
        int newsock = ::accept(sock_,nullptr,nullptr);
        // SYLAR_LOG_DEBUG(g_logger) << "Socket::~accept()";
        if(newsock == -1){
            SYLAR_LOG_ERROR(g_logger) << "accept(" << sock_ <<") errno="
                << errno << " errstr=" << strerror(errno);
            return nullptr;
        }
        if(sock->init(newsock)){
            return sock;
        }
        return nullptr;
    }

    bool Socket::init(int sock) 
    {
        FdCtx::ptr ctx = FdMgr::GetInstance()->get(sock);
        if(ctx && ctx->isSocket() && !ctx->isClose()){
            sock_ = sock;
            isConnected_ = true;    //有点疑问
            initSock();
            //unknown
            getLocalAddress();
            getRemoteAddress();
            return true;
        }
        return false;
    }
    bool Socket::bind(const Address::ptr addr) 
    {
        //socket 检测
        if(!isValid()){
            newSock();
            if(SYLAR_UNLICKLY(!isValid())){
                return false;
            }
        }

        if(SYLAR_UNLICKLY(addr->getFamily() != family_)){
            SYLAR_LOG_ERROR(g_logger) <<"bind sock.family(" 
                << family_ << ") addr.family("
                << addr->getFamily() << ") not equal, addr=" 
                << addr->toString();
                return false;
        }

        //  UnixAddress::ptr uaddr = std::dynamic_pointer_cast<UnixAddress>(addr);
        // if(uaddr) {
        //     Socket::ptr sock = Socket::CreateUnixTCPSocket();
        //     if(sock->connect(uaddr)) {
        //         return false;
        //     } else {
        //          sylar::FSUtil::Unlink(uaddr->getPath(), true);
        //     }
        // }

        if(::bind(sock_,addr->getAddr(),addr->getAddrLen())){
            SYLAR_LOG_ERROR(g_logger) << "bind errno=" <<errno
                << " errstr=" <<strerror(errno);
            return false;
        }
        // 初始化本地地址  服务端的socket没有remote Address
        getLocalAddress();
        return true;
    }

    bool Socket::connect(const Address::ptr addr, uint64_t timeout_ms) 
    {
        remoteAddress_ = addr;
        //socket 检测
        if(!isValid()){
            newSock();
            if(SYLAR_UNLICKLY(!isValid())){
                return false;
            }
        }

        if(SYLAR_UNLICKLY(addr->getFamily() != family_)){
            SYLAR_LOG_ERROR(g_logger) <<"connect sock.family(" 
                << family_ << ") addr.family("
                << addr->getFamily() << ") not equal, addr=" 
                << addr->toString();
            return false;
        }

        if(timeout_ms == (uint64_t)-1){
            SYLAR_LOG_DEBUG(g_logger) << "connect";
            if(::connect(sock_,addr->getAddr(),addr->getAddrLen())){
                SYLAR_LOG_ERROR(g_logger) << "sock=" << sock_
                    << " connect(" << addr->toString() <<") errno"
                    << errno <<" errstr=" <<strerror(errno);
                close();
                return false;
            }
        } else {
            SYLAR_LOG_DEBUG(g_logger) << "connect_with_timeout";
            if(::connect_with_timeout(sock_,addr->getAddr(),addr->getAddrLen(),timeout_ms)){
                SYLAR_LOG_ERROR(g_logger) << "sock=" << sock_
                    << " connect_with_timeout(" << addr->toString() <<") timout_ms="
                    << timeout_ms <<" errno"
                    << errno <<" errstr=" <<strerror(errno);
                close();
                return false;
            }
        }
        isConnected_ = true;
        getRemoteAddress();
        getLocalAddress();
        return true;
    }
    bool Socket::listen(int backlog) 
    {
        if(!isValid()){
            SYLAR_LOG_ERROR(g_logger) << "listen error sock=-1";
            return false;
        }
        if(::listen(sock_,backlog)){
            SYLAR_LOG_ERROR(g_logger) << "listen errno=" << errno
                << " errstr=" << strerror(errno);
            return false;
        }
        return true;
    }
    //带关闭的close
    bool Socket::close() 
    {
        if(!isConnected_ && sock_ == -1){
            return true;
        }
        isConnected_ = false;
        if(sock_ != -1){
            ::close(sock_);
            sock_ = -1;
        }
        //为什么是false
        return false;
    }

    //不带关闭功能的close
    bool Socket::isClose()const
    {
        if(!isConnected_ && sock_ == -1){
            return true;
        }
        return false;
    }

    int Socket::send(const void* buffers, size_t length, int flags ) 
    {
        if(isConnected()){
            return ::send(sock_,buffers,length,flags);
        }
        return -1;
    }
    int Socket::send(const iovec* buffers, size_t length, int flags ) 
    {
        if(isConnected()){
            msghdr msg;
            memset(&msg,0,sizeof(msg));
            msg.msg_iov = (iovec*)buffers;
            msg.msg_iovlen = length;
            return ::sendmsg(sock_,&msg,flags);
        }
        return -1;
    }
    // udp length buffers的数量
    int Socket::sendTo(const void *buffers, size_t length, const Address::ptr to, int flags ) 
    {
        if(isConnected()){
            return ::sendto(sock_,buffers,length,flags,to->getAddr(),to->getAddrLen());
        }
        return -1;
    }
    int Socket::sendTo(const iovec *buffers, size_t length, const Address::ptr to, int flags ) 
    {
        if(isConnected()){
            msghdr msg;
            memset(&msg,0,sizeof(msg));
            msg.msg_iov = (iovec*)buffers;
            msg.msg_iovlen = length;
            msg.msg_name = to->getAddr();
            msg.msg_namelen = to->getAddrLen();
            return ::sendmsg(sock_,&msg,flags);
        }
        return -1;
    }

    int Socket::recv(void *buffer, size_t length, int flags) 
    {
        if(isConnected()){
            return ::recv(sock_,buffer,length,flags);
        }    
        return -1;
    }

    int Socket::recv(iovec *buffer, size_t length, int flags ) 
    {
        if(isConnected()){
            msghdr msg;
            memset(&msg,0,sizeof(msg));
            msg.msg_iov = (iovec*)buffer;
            msg.msg_iovlen = length;
            return ::recvmsg(sock_,&msg,flags);
        }
        return -1;
    }
    // udp
    int Socket::recvFrom(void *buffer, size_t length, Address::ptr from, int flags ) 
    {
        if(isConnected()){
            socklen_t len = from->getAddrLen();
            return ::recvfrom(sock_,buffer,length,flags, from->getAddr(),&len);
        }
        return -1;
    }
    int Socket::recvFrom(iovec *buffer, size_t length, Address::ptr from, int flags ) 
    {
        if(isConnected()){
            msghdr msg;
            memset(&msg,0,sizeof(msg));
            msg.msg_iov = buffer,
            msg.msg_iovlen = length;
            msg.msg_name =  from->getAddr();
            msg.msg_namelen = from->getAddrLen();
            return ::recvmsg(sock_,&msg,flags);
        }
        return -1;
    }
    //初始化则返回 没有则创建
    Address::ptr Socket::getRemoteAddress() 
    {
        if(remoteAddress_){
            return remoteAddress_;
        }

        Address::ptr result;
        switch (family_)
        {
        case AF_INET:
        result.reset(new IPv4Address());
            break;
        case AF_INET6:
        result.reset(new IPv6Address());
            break;
        case AF_UNIX:
        result.reset(new UnixAddress());
            break;
        default:
        result.reset(new UnknownAddress(family_));
            break;
        }
        socklen_t addrlen = result->getAddrLen();
        if(getpeername(sock_, result->getAddr(),&addrlen)){
            SYLAR_LOG_ERROR(g_logger) << "getpeername sock=" << sock_
                << " errno=" << errno <<" errstr=" << strerror(errno);
            return Address::ptr(new UnknownAddress(family_));
        }
        if(family_ == AF_UNIX){
            UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
            addr->setAddrLen(addrlen);
        }
        remoteAddress_ = result;
        return remoteAddress_;
    }
    Address::ptr Socket::getLocalAddress() 
    {
        if(localAddress_){
            return localAddress_;
        }

        Address::ptr result;
        switch (family_)
        {
        case AF_INET:
        result.reset(new IPv4Address());
            break;
        case AF_INET6:
        result.reset(new IPv6Address());
            break;
        case AF_UNIX:
        result.reset(new UnixAddress());
            break;
        default:
        result.reset(new UnknownAddress(family_));
            break;
        }
        socklen_t addrlen = result->getAddrLen();
        if(getsockname(sock_, result->getAddr(),&addrlen)){
            SYLAR_LOG_ERROR(g_logger) << "getsockname sock=" << sock_
                << " errno=" << errno <<" errstr=" << strerror(errno);
            return Address::ptr(new UnknownAddress(family_));
        }
        if(family_ == AF_UNIX){
            UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
            addr->setAddrLen(addrlen);
        }
        localAddress_ = result;
        return localAddress_;
    }
    bool Socket::isValid() const 
    {
        return sock_ != -1;
    }

    //success 0   fail  -1
    int Socket::getError() 
    {
        int error = 0;
        size_t len = sizeof(error);
        if(!getOption(SOL_SOCKET,SO_ERROR,&error,&len)){
            return -1;
        }
        return error;
    }
    std::ostream& Socket::dump(std::ostream& os) const 
    {
        os << "[Socket sock=" << sock_
            << " is_connected" << isConnected_
            << " famaly="<< family_
            << " type="<< type_
            << " rm col="<< protocol_
            << " ";
            if(localAddress_){
                os << localAddress_->toString() << " ";
            }
            if(remoteAddress_){
                os << remoteAddress_->toString();
            }
            os << "]";
            return os;
    }
    bool Socket::cancelRead() 
    {
        return IOManager::GetThis()->cancelEvent(sock_,sylar::IOManager::READ);
    }
    bool Socket::cancelWrite() 
    {
        return IOManager::GetThis()->cancelEvent(sock_,sylar::IOManager::WRITE);
    }
    //为什么是读事件
    bool Socket::cancelAccept() 
    {
        return IOManager::GetThis()->cancelEvent(sock_,sylar::IOManager::READ);
    }

    bool Socket::cancelAll() 
    {
        return IOManager::GetThis()->cancelAll(sock_);
    }
    void Socket::initSock()
    {
        int val = 1;
        setOption(SOL_SOCKET,SO_REUSEADDR,val);
        if(type_ == SOCK_STREAM){
            setOption(IPPROTO_TCP,TCP_NODELAY,val);
        }
    }

    void Socket::newSock()
    {
        sock_ = socket(family_,type_,protocol_);
        if(SYLAR_LICKLY(sock_ != -1)){
            initSock();
        } else {
            SYLAR_LOG_ERROR(g_logger) << "socket(" << family_
                << ", " << type_ << ", " << protocol_ <<") errno="
                    << errno <<" errstr=" << strerror(errno);
        }
    }

    

    namespace {

    struct _SSLInit {
        _SSLInit() {
            SSL_library_init();
            SSL_load_error_strings();
            OpenSSL_add_all_algorithms();
        }
    };

    static _SSLInit s_init;

    }

    SSLSocket::SSLSocket(int family, int type, int protocol)
        :Socket(family, type, protocol) {
    }

    Socket::ptr SSLSocket::accept() {
        SSLSocket::ptr sock(new SSLSocket(family_, type_, protocol_));
        int newsock = ::accept(sock_, nullptr, nullptr);
        if(newsock == -1) {
            SYLAR_LOG_ERROR(g_logger) << "accept(" << sock_ << ") errno="
                << errno << " errstr=" << strerror(errno);
            return nullptr;
        }
        sock->ctx_ = ctx_;
        if(sock->init(newsock)) {
            return sock;
        }
        return nullptr;
    }

    bool SSLSocket::bind(const Address::ptr addr) {
        return Socket::bind(addr);
    }

    bool SSLSocket::connect(const Address::ptr addr, uint64_t timeout_ms) {
        bool v = Socket::connect(addr, timeout_ms);
        if(v) {
            ctx_.reset(SSL_CTX_new(SSLv23_client_method()), SSL_CTX_free);
            ssl_.reset(SSL_new(ctx_.get()),  SSL_free);
            SSL_set_fd(ssl_.get(), sock_);
            v = (SSL_connect(ssl_.get()) == 1);
        }
        return v;
    }

    bool SSLSocket::listen(int backlog) {
        return Socket::listen(backlog);
    }

    bool SSLSocket::close() {
        return Socket::close();
    }

    int SSLSocket::send(const void* buffer, size_t length, int flags) {
        if(ssl_) {
            return SSL_write(ssl_.get(), buffer, length);
        }
        return -1;
    }

    int SSLSocket::send(const iovec* buffers, size_t length, int flags) {
        if(!ssl_) {
            return -1;
        }
        int total = 0;
        for(size_t i = 0; i < length; ++i) {
            int tmp = SSL_write(ssl_.get(), buffers[i].iov_base, buffers[i].iov_len);
            if(tmp <= 0) {
                return tmp;
            }
            total += tmp;
            if(tmp != (int)buffers[i].iov_len) {
                break;
            }
        }
        return total;
    }

    int SSLSocket::sendTo(const void* buffer, size_t length, const Address::ptr to, int flags) {
        SYLAR_ASSERT(false);
        return -1;
    }

    int SSLSocket::sendTo(const iovec* buffers, size_t length, const Address::ptr to, int flags) {
        SYLAR_ASSERT(false);
        return -1;
    }

    int SSLSocket::recv(void* buffer, size_t length, int flags) {
        if(ssl_) {
            return SSL_read(ssl_.get(), buffer, length);
        }
        return -1;
    }

    int SSLSocket::recv(iovec* buffers, size_t length, int flags) {
        if(!ssl_) {
            return -1;
        }
        int total = 0;
        for(size_t i = 0; i < length; ++i) {
            int tmp = SSL_read(ssl_.get(), buffers[i].iov_base, buffers[i].iov_len);
            if(tmp <= 0) {
                return tmp;
            }
            total += tmp;
            if(tmp != (int)buffers[i].iov_len) {
                break;
            }
        }
        return total;
    }

    int SSLSocket::recvFrom(void* buffer, size_t length, Address::ptr from, int flags) {
        SYLAR_ASSERT(false);
        return -1;
    }

    int SSLSocket::recvFrom(iovec* buffers, size_t length, Address::ptr from, int flags) {
        SYLAR_ASSERT(false);
        return -1;
    }

    bool SSLSocket::init(int sock) {
        bool v = Socket::init(sock);
        if(v) {
            ssl_.reset(SSL_new(ctx_.get()),  SSL_free);
            SSL_set_fd(ssl_.get(), sock_);
            v = (SSL_accept(ssl_.get()) == 1);
        }
        return v;
    }

    bool SSLSocket::loadCertificates(const std::string& cert_file, const std::string& key_file) {
        ctx_.reset(SSL_CTX_new(SSLv23_server_method()), SSL_CTX_free);
        if(SSL_CTX_use_certificate_chain_file(ctx_.get(), cert_file.c_str()) != 1) {
            SYLAR_LOG_ERROR(g_logger) << "SSL_CTX_use_certificate_chain_file("
                << cert_file << ") error";
            return false;
        }
        if(SSL_CTX_use_PrivateKey_file(ctx_.get(), key_file.c_str(), SSL_FILETYPE_PEM) != 1) {
            SYLAR_LOG_ERROR(g_logger) << "SSL_CTX_use_PrivateKey_file("
                << key_file << ") error";
            return false;
        }
        if(SSL_CTX_check_private_key(ctx_.get()) != 1) {
            SYLAR_LOG_ERROR(g_logger) << "SSL_CTX_check_private_key cert_file="
                << cert_file << " key_file=" << key_file;
            return false;
        }
        return true;
    }

    SSLSocket::ptr SSLSocket::CreateTCP(sylar::Address::ptr address) {
        SSLSocket::ptr sock(new SSLSocket(address->getFamily(), TCP, 0));
        return sock;
    }

    SSLSocket::ptr SSLSocket::CreateTCPSocket() {
        SSLSocket::ptr sock(new SSLSocket(IPv4, TCP, 0));
        return sock;
    }

    SSLSocket::ptr SSLSocket::CreateTCPSocket6() {
        SSLSocket::ptr sock(new SSLSocket(IPv6, TCP, 0));
        return sock;
    }

    std::ostream& SSLSocket::dump(std::ostream& os) const {
        os << "[SSLSocket sock=" << sock_
        << " is_connected=" << isConnected_
        << " family=" << family_
        << " type=" << type_
        << " protocol=" << protocol_;
        if(localAddress_) {
            os << " local_address=" << localAddress_->toString();
        }
        if(remoteAddress_) {
            os << " remote_address=" << remoteAddress_->toString();
        }
        os << "]";
        return os;
    }

    std::ostream& operator<<(std::ostream& os, const Socket& sock){
        sock.dump(os);
        return os;
    } 

}//sylar