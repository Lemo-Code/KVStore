#include "sylar/tcp_server.h"
#include "sylar/config.h"
#include "sylar/hook.h"
namespace sylar
{

static sylar::ConfigVar<uint64_t>::ptr g_tcp_server_read_tiemout =  
    sylar::Config::Lookup("tcp_server.read_timeout",(uint64_t)(60 * 1000 * 2),
                            "tcp server read timeout");

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

TcpServer::TcpServer(sylar::IOManager* work,
                sylar::IOManager* io_worker,
                sylar::IOManager* acceptWorker)
    :worker_(work)
    ,io_worker_(io_worker)
    ,acceptWorker_(acceptWorker)
    ,recvTimeout_(g_tcp_server_read_tiemout->getValue())
    ,name_("sylar/1.0.0")
    ,m_ssl(false)
    ,isStop_(true){
}

TcpServer::~TcpServer(){
    SYLAR_LOG_INFO(g_logger) << "tcp_server end";
    for(auto &i :socks_){
        i->close();
    }
    socks_.clear();
}

bool TcpServer::bind(sylar::Address::ptr addr){
    std::vector<Address::ptr> addrs;
    std::vector<Address::ptr> fails;
    addrs.push_back(addr);
    return bind(addrs,fails);
}

bool TcpServer::bind(std::vector<Address::ptr>& addrs,std::vector<Address::ptr>& fail_addrs){
    for(auto &addr : addrs){
        Socket::ptr sock = Socket::CreateTCP(addr);
        if(!sock->bind(addr)){
            SYLAR_LOG_ERROR(g_logger) << "bind fail errno="
                << errno << " errstr=" << strerror(errno) 
                << " addr=[" << addr->toString() << "]";
            fail_addrs.push_back(addr);
            continue;
        }
        if(!sock->listen()){
            SYLAR_LOG_ERROR(g_logger) << "listen fail errno="
                << errno << " errstr=" << strerror(errno)
                << " addr=[" << addr->toString() << "]";
            fail_addrs.push_back(addr);
            continue;
        }
        socks_.push_back(sock);
    }

    if(!fail_addrs.empty()){
        socks_.clear();
        return false;
    }

    for(auto & i : socks_){
        SYLAR_LOG_INFO(g_logger) << "Server bind success: " << *i;
    }
    return true;
}

bool TcpServer::start(){
    if(!isStop_){
        //已经开启
        return true;
    }
    isStop_ = false;
    for(auto& sock : socks_){
        acceptWorker_->schedule(std::bind(&TcpServer::startAccept,
                    shared_from_this(),sock));
    }
    return true;
}

void TcpServer::stop(){
    isStop_ = true;
    auto self = shared_from_this();
    acceptWorker_->schedule([this,self]() {
        for(auto& i : socks_){
            //i的任务就是listen 唤醒的也是listen
            i->cancelAll();
            i->close();
        }
        socks_.clear();
    });
}
    
void TcpServer::handleClient(Socket::ptr client){
    // std::string res = "server smer/1.0";
    //     std::string res = "HTTP/1.1 200 OK\t\n"
    // "connection: close\r\n"
    // "content-length: 19\r\n\r\n"
    // "hello, this is smer";
    //     client->send(res.c_str(),res.size());
    // SYLAR_LOG_INFO(g_logger) << "handleClient ...xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
}

std::string TcpServer::toString(const std::string& prefix) {
    std::stringstream ss;
    ss << prefix << "[type=" << m_type
       << " name=" << name_ << " ssl=" << m_ssl
       << " worker=" << (worker_ ? worker_->getName() : "")
       << " accept=" << (acceptWorker_ ? acceptWorker_->getName() : "")
       << " recv_timeout=" << recvTimeout_ << "]" << std::endl;
    std::string pfx = prefix.empty() ? "    " : prefix;
    for(auto& i : socks_) {
        ss << pfx << pfx << *i << std::endl;
    }
    return ss.str();
}

void TcpServer::startAccept(Socket::ptr sock){
    while(!isStop_){
        Socket::ptr client = sock->accept();
        if(client){
            client->setRecvTimeout(recvTimeout_);
            worker_->schedule(std::bind(&TcpServer::handleClient,
                        shared_from_this(),client));
        } else {
            SYLAR_LOG_ERROR(g_logger) << "accept errno=" << errno
                << " errstr=" << strerror(errno);
        }
    }
}

}//sylar