#include "sylar/daemon.h"
#include "sylar/log.h"
#include "sylar/config.h"
#include "sylar/hook.h"

#include <time.h>
#include <string.h>
#include <sys/wait.h>

namespace sylar
{
static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");
static sylar::ConfigVar<uint32_t>::ptr g_daemon_restart_interval
    = sylar::Config::Lookup("daemon.restart_interval", (uint32_t)5, "daemon restart interval");

std::string ProcessInfo::toString(){
    std::stringstream ss;
    ss << "[ProcessInfo parent_id=" << parent_id
        << " main_id=" << main_id 
        << " parent_start_time=" << sylar::Time2Str(parent_start_time)
        << " main_start_time=" << sylar::Time2Str(main_start_time)
        << " restart_count=" << restart_count << "]";
    return ss.str();
}

//非守护进程的方式
static int real_start(int argc, char** argv, std::function<int(int argc,char** argv)> main_cb) {
    return main_cb(argc,argv);
}
//守护进程的方式
static int real_daemon(int argc, char** argv, 
                std::function<int(int argc,char** argv)> main_cb) {
    if(daemon(1,0)){
        SYLAR_LOG_FATAL(g_logger) << "daemon error";
    }
    ProcessInfoMgr::GetInstance()->parent_id = getpid();
    ProcessInfoMgr::GetInstance()->parent_start_time = time(0);
    while (true)
    {
        pid_t pid = fork();
        if(pid == 0){
            //子进程返回
            ProcessInfoMgr::GetInstance()->main_id = getpid();
            ProcessInfoMgr::GetInstance()->main_start_time = time(0);
            SYLAR_LOG_INFO(g_logger) << "process start pid=" << getpid();
            return real_start(argc, argv, main_cb);
        } else if(pid < 0){
            SYLAR_LOG_ERROR(g_logger) << "fork fail return=" << pid
                <<" error=" << errno << " errstr=" << strerror(errno);
            return -1;
        } else {
            //父进程返回
            int status = 0;
            waitpid(pid, &status , 0);
            if(status){
                SYLAR_LOG_ERROR(g_logger) << "child crash pid=" << pid
                    << " status=" << status;
            } else {//正常退出
                SYLAR_LOG_INFO(g_logger) << "child finished pid=" << pid;
                break;
            }
            ProcessInfoMgr::GetInstance()->restart_count += 1;
            // SYLAR_LOG_INFO(g_logger) << "xxx";
            //由于我的系统就是默认所有线程都是hook的，所以这里使用原函数
            //我们希望的是阻塞2秒，等待系统对资源准备完成，而不是设置异步事件，
            //当该线程是hook线程的时候，子线程已经退出了，其中已经没有资源了，你还利用IOManager去设置定时事件，访问空指针必出core
            //还有一个问题： 为什么 设置全局hook以后 杀死进程，就会出现core
            sleep(g_daemon_restart_interval->getValue());
            // SYLAR_LOG_INFO(g_logger) << "====";
        }
    }//while
    return 0;
}

int start_daemon(int argc, char** argv
                , std::function<int(int argc,char** argv)> main_cb
                , bool is_daemon){
    if(!is_daemon){
        return real_start(argc, argv, main_cb);
    }
    return real_daemon(argc,argv,main_cb);
}
}