#include "sylar/hook.h"
#include "sylar/log.h"
#include "sylar/config.h"
#include "sylar/iomanager.h"
#include "sylar/fdmanager.h"
#include "sylar/fiber.h"
#include <stdarg.h>
#include <dlfcn.h>
#include <time.h>
#include <atomic>

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");
namespace sylar
{

//静态非静态区别  作用域是文件内部

static sylar::ConfigVar<int>::ptr g_tcp_connect_timeout =
    sylar::Config::Lookup("tcp.connect_timeout",5000,"tcp connect timeout");

//可能有的线程进入的时候是false的，直接卡死  所以如果想设置某个线程不是异步的 可以设置为false
// 如果真想不使用异步就直接使用原函数吧 或者把set_hook_enable 调到thread的run中 而不是调度器的run中
//解决方案 ： 主线程也要设置hook选项
static bool thread_local  t_hook_enable = false;

 // 本线程 是否hook
     bool is_hook_enable()
     {
        return t_hook_enable;
     }
     // set_hook_enable
     void set_hook_enable(bool flag)
     {
        t_hook_enable = flag;
     }

#define HOOK_FUN(XX) \
    XX(sleep) \
    XX(usleep) \
    XX(nanosleep) \
    XX(socket) \
    XX(connect) \
    XX(accept) \
    XX(read) \
    XX(readv) \
    XX(recv) \
    XX(recvfrom) \
    XX(recvmsg) \
    XX(write) \
    XX(writev) \
    XX(send) \
    XX(sendto) \
    XX(sendmsg) \
    XX(close) \
    XX(fcntl) \
    XX(ioctl) \
    XX(getsockopt) \
    XX(setsockopt)   
    
    void hook_init()
    {
        static bool is_init = false;
        if(is_init){
            return;
        }
//进行函数拦截  把系统函数名 变更成带后缀的函数名  dlsym-2
#define XX(name) name ## _f = (name ## _fun)dlsym(RTLD_NEXT, #name); 
        HOOK_FUN(XX)
#undef XX

    }

static uint64_t s_connect_timeout = -1;
    struct __HookIniter{
        __HookIniter(){
            hook_init();
            s_connect_timeout = g_tcp_connect_timeout->getValue();

            g_tcp_connect_timeout->addListener([](const int& old_value, const int& new_value){
                SYLAR_LOG_INFO(g_logger) << "tcp connect timout changed from " 
                << old_value <<" to " << new_value;
                s_connect_timeout = new_value;
            });
        }
    };

    //进入main函数之前完成构造 
    static __HookIniter s_hook_initer;

     

}

struct timer_info
    {
        int cancelled = 0;
    };
// int fd = do_io(sockfd , accept_f,"accept", sylar::IOManager::READ,SO_RCVTIMEO,addr,addrlen);
    template <typename OriginFun, typename... Args>
     static ssize_t do_io(int fd, OriginFun fun, const char *hook_fun_name,
                uint32_t event, int time_out_so, Args &&... args){
        //说明不是hook 线程 (question)
        if (!sylar::t_hook_enable){
             return fun(fd, std::forward<Args>(args)...);
         }

         //说明不是socket的文件句柄 使用原函数 （没懂）
         sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
         if (!ctx){ // 使用原方法
            return fun(fd, std::forward<Args>(args)...);
         }
         //文件关闭
         if (ctx->isClose()){
             errno = EBADF;
             return -1;
         }
         //不是socket 或者 用户已经设置了nonblock
         if (!ctx->isSocket() || ctx->getUserNonBlock()){
             return fun(fd, std::forward<Args>(args)...);
         }
 
         uint64_t to = ctx->getTimeout(time_out_so);
         // 创建条件
         std::shared_ptr<timer_info> tinfo(new timer_info);
 
     RETRY:
         ssize_t n = fun(fd, std::forward<Args>(args)...);
         //信号中断
         while (n == -1 && errno == EINTR)
         {
             n = fun(fd, std::forward<Args>(args)...);
         }
         //重新加入到任务队列  资源不可用
         if (n == -1 && errno == EAGAIN)
         {
            //丢到调度器中
             sylar::IOManager *iom = sylar::IOManager::GetThis();
             sylar::Timer::ptr timer;
             std::weak_ptr<timer_info> winfo(tinfo);
 
             if (to != (uint64_t)-1)
             {//question
                 timer = iom->addConditionTimer(to, [winfo, fd, iom, event]() {
                     auto t = winfo.lock();
                     if(!t || t->cancelled){
                         return;
                     }
                     t->cancelled = ETIMEDOUT;
                     iom->cancelEvent(fd,(sylar::IOManager::Event)(event)); 
                }, winfo);
             }
             int rt = iom->addEvent(fd, (sylar::IOManager::Event)(event));
             //事件加上了
             if(rt)
             { 
                 SYLAR_LOG_ERROR(g_logger) << hook_fun_name << "addEvent("
                                           << fd << ", " << event << ") ";
                 if (timer)
                 {
                     timer->cancel();
                 }
                 return -1;
             }
             else
             { // ToHold和ToReady的区别(最终应该返回当前线程的主协程) 但是并不是做最终结果
                 sylar::Fiber::YeildToHold();//(当前协程回到run)
                 if (timer)
                 {
                     timer->cancel();
                 }
                 if (tinfo->cancelled)
                 {
                     errno = tinfo->cancelled;
                     return -1;
                 }
                goto RETRY;
             }
         }
         return n;
     }

extern "C"
{
//初始化函数指针变量 dlsym-1
#define XX(name) name ## _fun name ## _f = nullptr;  
    HOOK_FUN(XX)
#undef XX
    
    //秒级
    unsigned int sleep(unsigned int seconds)
    {
        //不是 hook线程 就执行原系统函数
        if(!sylar::t_hook_enable){
            return sleep_f(seconds);
        }
        sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
        sylar::IOManager* iom = sylar::IOManager::GetThis();
        //定时器是毫秒级 seconds秒执行完任务了才回来YeildToHold
         iom->addTimer(seconds,std::bind((void(sylar::Scheduler::*)
            (sylar::Fiber::ptr,int thread))&sylar::IOManager::schedule,
            iom,fiber,-1));
        sylar::Fiber::YeildToHold();
        return 0;
    }

    //微秒级 添加一个定时器 当时间到了会进行一次通知的 （怎么个流程 没懂）
    int usleep(useconds_t usec)
    {
         //不是 hook线程 就执行原系统函数
         if(!sylar::t_hook_enable){
            return usleep_f(usec);
        }
        sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
        sylar::IOManager* iom = sylar::IOManager::GetThis();
        //定时器是毫秒级
         iom->addTimer(usec / 1000,std::bind((void(sylar::Scheduler::*)
            (sylar::Fiber::ptr,int thread))&sylar::IOManager::schedule,
            iom,fiber,-1));
        sylar::Fiber::YeildToHold();
        return 0;
    }

    //
    int nanosleep(const struct timespec *req, struct timespec *rem)
    {
        //不是 hook线程 就执行原系统函数
        if(!sylar::t_hook_enable){
            return nanosleep_f(req,rem);
        }

        int timeout_ms = req->tv_sec * 1000 + req->tv_nsec /1000 /1000;
        sylar::Fiber::ptr fiber = sylar::Fiber::GetThis();
        sylar::IOManager* iom = sylar::IOManager::GetThis();
        //定时器是毫秒级 seconds秒执行完任务了才回来YeildToHold
        iom->addTimer(timeout_ms,std::bind((void(sylar::Scheduler::*)
            (sylar::Fiber::ptr,int thread))&sylar::IOManager::schedule,
            iom,fiber,-1));
        sylar::Fiber::YeildToHold();
        return 0;
    }
    
   /**
    * Read IO
    */
    ssize_t read(int fd, void *buf, size_t count)
    {
        return do_io(fd , read_f,"read",sylar::IOManager::READ,SO_RCVTIMEO,buf,count);    
    }
    ssize_t readv(int fd, const struct iovec *iov, int iovcnt)
    {
        return do_io(fd , readv_f,"readv",sylar::IOManager::READ,SO_RCVTIMEO,iov,iovcnt);  
    }

    ssize_t recv(int sockfd, void *buf, size_t len, int flags)
    {
        return do_io(sockfd , recv_f,"recv",sylar::IOManager::READ,SO_RCVTIMEO,buf,len,flags);  
    }

    ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,struct sockaddr *src_addr, socklen_t *addrlen)
    {
        return do_io(sockfd , recvfrom_f,"recvfrom",sylar::IOManager::READ,SO_RCVTIMEO,buf,len,flags,src_addr,addrlen);  
    }

    ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags)
    {
        return do_io(sockfd , recvmsg_f,"recvmsg",sylar::IOManager::READ,SO_RCVTIMEO,msg,flags);  
    }
  /**
    * Write IO
    */
    ssize_t write(int fd, const void *buf, size_t count)
    {
        return do_io(fd , write_f,"write",sylar::IOManager::WRITE,SO_SNDTIMEO,buf,count);  
    }
    ssize_t writev(int fd, const struct iovec *iov, int iovcnt)
    {
        return do_io(fd , writev_f,"writev",sylar::IOManager::WRITE,SO_SNDTIMEO,iov,iovcnt);  
    }
    ssize_t send(int sockfd, const void *buf, size_t len, int flags)
    {
        return do_io(sockfd , send_f,"send",sylar::IOManager::WRITE,SO_SNDTIMEO,buf,len,flags);  
    }
    ssize_t sendto(int sockfd, const void *buf, size_t len,int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
    {
        return do_io(sockfd , sendto_f,"sendto",sylar::IOManager::WRITE,SO_SNDTIMEO,buf,len,flags,dest_addr,addrlen);  
    }
    ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags)
    {
        return do_io(sockfd , sendmsg_f,"sendmsg",sylar::IOManager::WRITE,SO_SNDTIMEO,msg,flags);  
    }

    /**
     *  socket
     */
    int socket(int domain, int type, int protocol)
    {
         //不是 hook线程 就执行原系统函数
         if(!sylar::t_hook_enable){
            return socket_f(domain,type,protocol);
        }

        int fd = socket_f(domain,type,protocol);
        if(fd == -1){
            return fd;
        }
        //有则返回 没有创建并存入容器
        sylar::FdMgr::GetInstance()->get(fd,true);
        return fd;
    }

    int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
    {
        int fd = do_io(sockfd , accept_f,"accept", sylar::IOManager::READ,SO_RCVTIMEO,addr,addrlen);
        if(fd >= 0){
            sylar::FdMgr::GetInstance()->get(fd,true);
        }
        // SYLAR_LOG_INFO(g_logger) << "accept return fd:" << fd << "    " <<sockfd;
        return fd;
    }

    //连接数据库可用
    int connect_with_timeout(int fd ,const struct sockaddr* addr,socklen_t addrlen,uint64_t timeout_ms)
    {
        if(!sylar::t_hook_enable){
            return connect_f(fd,addr,addrlen);
        }
        sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
        if(!ctx || ctx->isClose()){
            errno = EBADF;
            return -1;
        }

        if(!ctx->isSocket()){
            return connect_f(fd,addr,addrlen);
        }
        if(ctx->getUserNonBlock()){
            return connect_f(fd,addr,addrlen);
        }
        int n = connect_f(fd,addr,addrlen);
        if(n == 0){
            return 0;
        }else if(n != -1 || errno != EINPROGRESS){
            //表示连接正在进行中，程序可以继续执行其他任务，之后再通过适当的方式检查连接状态。
            return n;
        }
        sylar::IOManager* iom = sylar::IOManager::GetThis();
        sylar::Timer::ptr timer;
        std::shared_ptr<timer_info> tinfo(new timer_info);
        std::weak_ptr<timer_info> winfo(tinfo);
        
        if(timeout_ms != (uint64_t)-1){
            //没有超时
            timer = iom->addConditionTimer(timeout_ms,[winfo,fd,iom]() {
                auto t = winfo.lock();
                if(!t || t->cancelled){
                    return;
                }
                t->cancelled = ETIMEDOUT;
                iom->cancelEvent(fd,sylar::IOManager::WRITE);
            },winfo);
        }
        int rt = iom->addEvent(fd,sylar::IOManager::WRITE);
        if(rt == 0){
            sylar::Fiber::YeildToHold();
            if(timer){
                timer->cancel();
            }
            if(tinfo->cancelled){
                errno = tinfo->cancelled;
                return -1;
            }
        }else{
            if(timer){
                timer->cancel();
            }
            SYLAR_LOG_ERROR(g_logger) << "connect addEvent (" << fd <<"WRITE) error";
        }
        int error = 0;
        socklen_t len = sizeof(int);
        if(-1 == getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&len)){
            return -1;
        }
        if(!error){
            return 0;
        }else{
            errno =error;
            return -1;
        }
    }

    int connect(int sockfd, const struct sockaddr *addr,socklen_t addrlen)
    {
        return connect_with_timeout(sockfd,addr,addrlen,sylar::s_connect_timeout);
    }
 
    /**
     * system
     */
    int close(int fd)
    {
        if(!sylar::t_hook_enable){
            return close_f(fd);
        }
        sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
        if(ctx){
            auto iom = sylar::IOManager::GetThis();
            if(iom){
                iom->cancelAll(fd);
            }
            sylar::FdMgr::GetInstance()->del(fd);
        }
        return close_f(fd);
    }

    int fcntl(int fd, int cmd, ... /* arg */)
    {
        if(!sylar::t_hook_enable){
            return fcntl_f(fd,cmd);
        }
        va_list va;
        va_start(va,cmd);
        switch(cmd)
        {
            case F_SETFL:
                {
                    int arg = va_arg(va,int);
                    va_end(va);
                    sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
                    if(!ctx || ctx->isClose() || !ctx->isSocket())//不存在 或 关闭 或 非socket
                    {
                        return fcntl_f(fd,cmd,arg);
                    }
                    ctx->setUserNonBlock(arg & O_NONBLOCK);
                    if(ctx->getSysNonBlock()){
                        arg |= O_NONBLOCK;
                    }else{
                        arg &= ~O_NONBLOCK;
                    }
                    return fcntl_f(fd,cmd,arg);
                }
            case F_DUPFD:
            case F_DUPFD_CLOEXEC:
            case F_SETFD:
            case F_SETOWN:
            case F_SETSIG:
            case F_SETLEASE:
            case F_NOTIFY:
            case F_SETPIPE_SZ:
                {
                    int arg = va_arg(va,int);
                    va_end(va);
                    return fcntl_f(fd,cmd,arg);
                }
                break;

            case F_GETFL:
                {
                    va_end(va);
                    int arg = fcntl_f(fd,cmd);
                    sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
                    if(!ctx || ctx->isClose() || !ctx->isSocket()){
                        return arg;
                    }
                    if(ctx->getSysNonBlock()){
                        return arg | O_NONBLOCK;
                    } else {
                        return arg & ~O_NONBLOCK;
                    }
                }
            case F_GETFD:
            case F_GETOWN: 
            case F_GETSIG:
            case F_GETLEASE:
            case F_GETPIPE_SZ:
                {
                    va_end(va);
                    return fcntl_f(fd,cmd);
                }
                break;

            case F_SETLK:
            case F_SETLKW:
            case F_GETLK:  
                {
                    struct flock* arg = va_arg(va,struct flock*);
                    va_end(va);
                    return fcntl_f(fd,cmd,arg);
                }
                break;
            case F_GETOWN_EX: 
            case F_SETOWN_EX:
                {
                    struct f_owner_exlock* arg = va_arg(va,struct f_owner_exlock*);
                    va_end(va);
                    return fcntl_f(fd,cmd,arg);
                }
                break;
            default:
                {
                    va_end(va);
                    return fcntl_f(fd,cmd);
                }
        }
    }
    
    int ioctl(int fd, unsigned long request, ...)
    {
        va_list va;
        va_start(va,request);
        void* arg = va_arg(va,void*);
        va_end(va);
        
        if(FIONBIO == request){
            bool user_nonblock = !!*(int*)arg;
            sylar::FdCtx::ptr ctx = sylar::FdMgr::GetInstance()->get(fd);
            if(!ctx || ctx->isClose() || !ctx->isSocket()){
                return ioctl_f(fd,request,arg);
            }
            ctx->setUserNonBlock(user_nonblock);
        }
        return ioctl_f(fd,request,arg);
    }

    int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen)
    {
        return getsockopt_f(sockfd,level,optname,optval,optlen);
    }

    int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
    {
        if(!sylar::t_hook_enable){
            return setsockopt_f(sockfd,level,optname,optval,optlen);
        }

        return setsockopt_f(sockfd,level,optname,optval,optlen);
    }


}
