#ifndef __SYLAR_TCP_SERVER_H__
#define __SYLAR_TCP_SERVER_H__
#include "sylar/iomanager.h"
#include "sylar/socket.h"
#include "sylar/address.h"
#include "sylar/noncopyable.h"
#include "sylar/lexicalcast.h"

#include <memory>
#include <functional>
#include <vector>
#include <string>

namespace sylar
{
//对httpServer配置进行特例化
struct TcpServerConf {
    std::vector<std::string> address;
    int keepalive = 1;
    int timeout = 1000 * 2 * 60;
    std::string name;
    std::string type;
    bool isValid()const {
        return !address.empty();
    }
    bool operator==(const TcpServerConf& other) const{
        return address == other.address
                && keepalive == other.keepalive
                && name == other.name
                && timeout == other.timeout;
    }
};

template<>
class LexicalCast<std::string,TcpServerConf>{
public:
    TcpServerConf operator()(const std::string& v){
        YAML::Node node = YAML::Load(v);
        TcpServerConf conf;
        conf.keepalive = node["keepalive"].as<int>();
        conf.timeout = node["timeout"].as<int>();
        conf.name = node["name"].as<std::string>();
        conf.type = node["type"].as<std::string>();
        if(node["address"].IsDefined()){
            for(size_t i = 0 ; i < node["address"].size();i++){
                conf.address.push_back(node["address"][i].as<std::string>());
            }
        }
        return conf;
    }
};

template<>
class LexicalCast<TcpServerConf, std::string>{
public:
    std::string operator()(const TcpServerConf& v){
        YAML::Node node;
        node["keepalive"] = v.keepalive;
        node["timeout"] = v.timeout;
        node["name"] = v.name;
        node["type"] = v.type;
        for(auto &i : v.address){
            node["address"].push_back(i);
        }
        std::stringstream ss;
        ss << node;
        return ss.str();
    }
};

class TcpServer :public std::enable_shared_from_this<TcpServer>
                , Noncopyable {
public:
    typedef std::shared_ptr<TcpServer> ptr;
    TcpServer(sylar::IOManager* work = sylar::IOManager::GetThis()
            , sylar::IOManager* io_woker = sylar::IOManager::GetThis()
            , sylar::IOManager* acceptWorker = sylar::IOManager::GetThis());
    virtual ~TcpServer();
    
    virtual bool bind(sylar::Address::ptr addr);
    virtual bool bind(std::vector<Address::ptr>& addrs,std::vector<Address::ptr>& fail_addrs);
    virtual bool start();
    virtual void stop();

    uint64_t getReadTimeout()const {return recvTimeout_;}
    std::string getName()const {return name_;}
    void setReadTimeout(uint64_t v) {recvTimeout_ = v;}
    virtual void setName(const std::string& v) {name_ = v;}

    bool isStop() const {return isStop_;}
    const TcpServerConf& getConf() const { return conf_;}
    virtual std::string toString(const std::string& prefix = "");
    void setConf(const TcpServerConf& v) {conf_ = v;}
    //初始化handler(项目分离的时候可以这么写，如果所有服务器一起启动，不适合这种方法)
    virtual void initHandler(){}
protected:
    virtual void handleClient(Socket::ptr client);
    virtual void startAccept(Socket::ptr sock);
protected:
    std::vector<Socket::ptr> socks_;    //listen监听的套接字 
    IOManager* worker_;         //  v
    IOManager* io_worker_;      //不做区分
    IOManager* acceptWorker_;   //  ^
    uint64_t recvTimeout_;      //timeout
    std::string name_;          //服务器名字
    std::string m_type;         //服务器类型
    bool m_ssl;                 //支持ssl
    bool isStop_;               //Server是否停止
    TcpServerConf conf_;        //配置数据
};

}

#endif