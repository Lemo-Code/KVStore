#ifndef __SYLAR_APPLICATION_H__
#define __SYLAR_APPLICATION_H__

#include "sylar/http/http_server.h"
#include <vector>
namespace sylar
{

class Application
{
public:
    Application();
    static Application* GetInstance(){return s_instance;}
    bool init(int argc, char** argv);
    bool run();
    //服务器框架真正执行的入口
    int main(int argc, char**argv);
    int run_fiber();
    //需要优化
    bool getServer(const std::string& name, std::vector<sylar::TcpServer::ptr>& srvs)const;
private:
    int argc_ = 0;
    char** argv_ = nullptr;
    sylar::IOManager::ptr main_manager;
    static Application* s_instance;
    std::vector<sylar::TcpServer::ptr> servers_;
};

}

#endif