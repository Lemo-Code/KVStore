#include "sylar/application.h"
#include "sylar/config.h"
#include "sylar/env.h"
#include "sylar/log.h"
#include "sylar/daemon.h"
#include "sylar/iomanager.h"
#include "sylar/address.h"
#include "sylar/http/http_server.h"
#include "sylar/http/ws_server.h"
#include "sylar/module.h"
#include "sylar/worker.h"

#include <string>
#include <stdlib.h>


namespace sylar
{
static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

static sylar::ConfigVar<std::string>::ptr g_server_work_path 
    = sylar::Config::Lookup("server.work_path"
                    , std::string("/home/smer/smer-git/chat/bin")
                    , "server work path");

static sylar::ConfigVar<std::string>::ptr g_server_pid_file 
    = sylar::Config::Lookup("server.pid_file"
                , std::string("sylar.pid")
                , "server pid file");
        

//http服务器的配置
static sylar::ConfigVar<std::vector<TcpServerConf> >::ptr g_http_server_conf
        = sylar::Config::Lookup("servers",
                                std::vector<TcpServerConf>(),
                                "http servers conf");


Application* Application::s_instance = nullptr;

Application::Application(){
    s_instance = this;
}

bool Application::init(int argc, char** argv){
    argc_ = argc;
    argv_ = argv;

    sylar::EnvMgr::GetInstance()->addHelp("s", "start with the terminal");
    sylar::EnvMgr::GetInstance()->addHelp("d", "run as daemon");
    sylar::EnvMgr::GetInstance()->addHelp("c", "conf path default: ./conf");
    sylar::EnvMgr::GetInstance()->addHelp("p", "print help");

    bool is_print_help = false;
    if(!sylar::EnvMgr::GetInstance()->init(argc, argv)) {
        is_print_help = true;
    }

    if(sylar::EnvMgr::GetInstance()->has("p")) {
        is_print_help = true;
    }

    std::string conf_path = sylar::EnvMgr::GetInstance()->getConfigPath();
    SYLAR_LOG_INFO(g_logger) << "load conf path:" << conf_path;
    sylar::Config::LoadFromConfDir(conf_path);

    ModuleMgr::GetInstance()->init();
    std::vector<Module::ptr> modules;
    ModuleMgr::GetInstance()->listAll(modules);

    for(auto i : modules) {
        i->onBeforeArgsParse(argc, argv);
    }

    if(is_print_help) {
        sylar::EnvMgr::GetInstance()->printHelp();
        return false;
    }

    for(auto i : modules) {
        i->onAfterArgsParse(argc, argv);
    }
    modules.clear();

    int run_type = 0;
    if(sylar::EnvMgr::GetInstance()->has("s")) {
        run_type = 1;
    }
    if(sylar::EnvMgr::GetInstance()->has("d")) {
        run_type = 2;
    }

    if(run_type == 0) {
        sylar::EnvMgr::GetInstance()->printHelp();
        return false;
    }

    std::string pidfile = g_server_work_path->getValue()
                                + "/" + g_server_pid_file->getValue();
    if(sylar::FSUtil::IsRunningPidfile(pidfile)) {
        SYLAR_LOG_ERROR(g_logger) << "server is running:" << pidfile;
        return false;
    }

    if(!sylar::FSUtil::Mkdir(g_server_work_path->getValue())) {
        SYLAR_LOG_FATAL(g_logger) << "create work path [" << g_server_work_path->getValue()
            << " errno=" << errno << " errstr=" << strerror(errno);
        return false;
    }
    return true;
}

bool Application::run(){
    bool is_daemon = sylar::EnvMgr::GetInstance()->has("d");
    return start_daemon(argc_,argv_,
            std::bind(&Application::main,this,std::placeholders::_1
            ,std::placeholders::_2),is_daemon);
}

int Application::main(int argc, char**argv){
    std::string pidFile = g_server_work_path->getValue()
                            + "/"
                            + g_server_pid_file->getValue();

    std::ofstream ofs(pidFile);
    if(!ofs){
        SYLAR_LOG_ERROR(g_logger) << "open pidfile " << pidFile << " failed";
        return false; // question
    }
    //服务器运行的pid
    ofs << getpid();
    
    auto http_confs = g_http_server_conf->getValue();
    for(auto &i : http_confs){
        SYLAR_LOG_INFO(g_logger) << LexicalCast<TcpServerConf,std::string>()(i);
    }

    main_manager.reset(new sylar::IOManager(1,true,"main"));
    main_manager->schedule(std::bind(&Application::run_fiber,this));
    main_manager->addTimer(2000, [](){
            //SYLAR_LOG_INFO(g_logger) << "hello";
    }, true);
    main_manager->stop();
    return 0;
}

int Application::run_fiber(){
    std::vector<Module::ptr> modules;
    ModuleMgr::GetInstance()->listAll(modules);
    bool has_error = false;
    for(auto& i : modules) {
        if(!i->onLoad()) {
            SYLAR_LOG_ERROR(g_logger) << "module name="
                << i->getName() << " version=" << i->getVersion()
                << " filename=" << i->getFilename();
            has_error = true;
        }
    }
    if(has_error) {
        _exit(0);
    }

    sylar::WorkerMgr::GetInstance()->init();
    auto http_confs = g_http_server_conf->getValue();
    for(auto &i : http_confs){
        // SYLAR_LOG_INFO(g_logger) << LexicalCast<TcpServerConf,std::string>()(i);

        std::vector<Address::ptr> addrs;
        for(auto& a:i.address){
            size_t pos = a.find(":");
            if(pos == std::string::npos){
                addrs.push_back(UnixAddress::ptr(new UnixAddress(a)));
                continue;
            }

            int32_t port = atoi(a.substr(pos + 1).c_str());
            //127.0.0.1
            auto addr = sylar::IPAddress::Create(a.substr(0, pos).c_str(), port);
            if(addr) {
                addrs.push_back(addr);
                continue;
            }
            std::vector<std::pair<Address::ptr, uint32_t> > result;
            if(sylar::Address::GetInterFaceAddress(result,
                                        a.substr(0, pos))) {
                for(auto& x : result) {
                    auto ipaddr = std::dynamic_pointer_cast<IPAddress>(x.first);
                    if(ipaddr) {
                        ipaddr->setPort(atoi(a.substr(pos + 1).c_str()));
                    }
                    addrs.push_back(ipaddr);
                }
                continue;
            }

            auto aaddr = sylar::Address::LookupAny(a);
            if(aaddr) {
                addrs.push_back(aaddr);
                continue;
            }
            SYLAR_LOG_ERROR(g_logger) << "invalid address: " << a;
            _exit(0);
        }
//
        IOManager* accept_worker = sylar::IOManager::GetThis();
        IOManager* io_worker = sylar::IOManager::GetThis();
        IOManager* process_worker = sylar::IOManager::GetThis();

//
        sylar::TcpServer::ptr server;
        if(i.type == "http"){  
            server.reset(new sylar::http::HttpServer(i.keepalive,accept_worker,io_worker,process_worker));
        } else if(i.type == "ws" || i.type == "web_socket"){
             server.reset(new sylar::http::WSServer(accept_worker,io_worker,process_worker));
        } else {
            SYLAR_LOG_ERROR(g_logger) << "invalid server type=" << i.type
                << LexicalCast<TcpServerConf, std::string>()(i);
            _exit(0);
        }
        
        if(!i.name.empty()) {
            server->setName(i.name);
        }

        std::vector<Address::ptr> fails;
        if(!server->bind(addrs,fails)){
            for(auto &ad : fails){
                SYLAR_LOG_ERROR(g_logger) << "bind address fail:"
                    << *ad;
            }
            _exit(0);
        }

        server->setConf(i);

        servers_.push_back(server);//引用计数至少唯一 防止提前析构
    }

    for(auto& i : modules) {
        i->onServerReady();
    }

    for(auto &i : servers_){
        i->start();
    }

    for(auto& i : modules) {
        i->onServerUp();
    }
    return 0;
}

bool Application::getServer(const std::string& name,
        std::vector<sylar::TcpServer::ptr>& srvs)const{
            for(auto &i : servers_){
                if(i->getConf().type == name){
                    srvs.push_back(i);
                }
            }
        return !srvs.empty();
    }

}//sylar
