#include "sylar/iomanager.h"
#include "sylar/log.h"
#include <string.h>
#include <fcntl.h>
#include <error.h>

namespace sylar
{
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    ////////////////FdContext
    IOManager::FdContext::EventContext& IOManager::FdContext::getContext(Event event)
    {
        switch(event)
        {
            case IOManager::READ:
                return read;
            case IOManager::WRITE:
                return write; 
            default:
                SYLAR_ASSERT2(false,"getContext");    
        }
        throw std::invalid_argument("getContext invalid event");
    }

    //重置参数
    void IOManager::FdContext::resetContext(EventContext &ctx)
    {
        ctx.scheduler = nullptr;
        ctx.fiber.reset();
        ctx.cb = nullptr;
    }

    //添加任务
    void IOManager::FdContext::triggerEvent(Event event)
    {
        SYLAR_ASSERT(events & event);
        events = (Event)(events & ~event);
        EventContext& ctx = getContext(event);
        if(ctx.cb){
            ctx.scheduler->schedule(&ctx.cb);
        }else{
            ctx.scheduler->schedule(&ctx.fiber);
        }
        ctx.scheduler = nullptr;
        return;
    }


    //构造函数初始化 配置管道
    IOManager::IOManager(size_t threads,bool use_caller , const std::string& name)
        :Scheduler(threads,use_caller,name)
    {
        epfd_ = epoll_create(5000);
        SYLAR_ASSERT(epfd_ > 0);

        int rt = pipe(ticklefds_);  
        SYLAR_ASSERT(rt == 0);   

        epoll_event event;
        memset(&event,0,sizeof(epoll_event));
        event.events = EPOLLIN | EPOLLET;
        //把管道读端设置成句柄
        event.data.fd = ticklefds_[0];
        
        rt = fcntl(ticklefds_[0],F_SETFL,O_NONBLOCK);
        SYLAR_ASSERT(!rt);
        
        rt = epoll_ctl(epfd_,EPOLL_CTL_ADD,ticklefds_[0],&event);
        SYLAR_ASSERT(!rt);

        ContextResize(32);;
        
        //启动线程池
        start(); //在出创建Scheduler的作用域之前都是可以正常添加任务的
    }

    //析构函数 资源处理
    IOManager::~IOManager()   
    {
        stop();
        close(epfd_);
        close(ticklefds_[0]);
        close(ticklefds_[1]);
        for(size_t i = 0; i<fdContexts_.size();i++)
        {
            if(fdContexts_[i])
            {
                delete fdContexts_[i];
            }
        }
    }
    //重置FdContext的大小
    void IOManager::ContextResize(size_t size)
    {
        fdContexts_.resize(size);

        for (size_t i = 0; i < fdContexts_.size(); i++)
        {
            if (!fdContexts_[i])
            {
                fdContexts_[i] = new FdContext;
                fdContexts_[i]->fd = i;
            }
        }
    }
    // 0 success -1 error
    // 增加事件
    int IOManager::addEvent(int fd, Event event, std::function<void()> cb)
    {
        // SYLAR_LOG_INFO(g_logger) << "IOManager::AddEvent "<< fd  << " - " << event;
        FdContext* fd_ctx = nullptr;

        RWMutexType::ReadLock lock(mutex_);
        if((int)fdContexts_.size() > fd)
        {
            fd_ctx = fdContexts_[fd];
            lock.unlock();
        }
        else
        {
            lock.unlock();
            RWMutex::WriteLock lock2(mutex_);
            ContextResize(fd * 1.5);
            fd_ctx = fdContexts_[fd];
        }

        //对取出的fd_cxt进行操作
        FdContext::MutexType::Lock lock2(fd_ctx->mutex);
        if(SYLAR_UNLICKLY(fd_ctx->events & event))
        {
            SYLAR_LOG_ERROR(g_logger)<< "IOManager::addEvent assert fd = "<< fd
                        << " event = "<<event
                        <<" fd_ctx = "<<fd_ctx->events;
            SYLAR_ASSERT(!(fd_ctx->events & event));
        }

        int op = fd_ctx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
        epoll_event epevent;
        epevent.events = EPOLLET | fd_ctx->events | event;
        epevent.data.ptr = fd_ctx;

        // SYLAR_LOG_INFO(g_logger) << "fd_ctx->events: " <<fd_ctx->events;
        int rt = epoll_ctl(epfd_,op,fd,&epevent);
        if(rt)
        {
            SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << epfd_
                <<"," << op << "," << fd << "," << epevent.events
                <<"):"<< rt <<" ("<< errno << ") (" << strerror(errno)
                    << ")";
            return -1;
        }

        //待执行的event
        ++pendingEventCount_;
        fd_ctx->events = (Event)(fd_ctx->events | event);
        FdContext::EventContext& event_ctx = fd_ctx->getContext(event);
        //确保Event_ctx为空
        SYLAR_ASSERT(!event_ctx.scheduler
                    && !event_ctx.fiber
                    && !event_ctx.cb);

        event_ctx.scheduler = Scheduler::GetThis();
        if(cb){
           // cb();
            event_ctx.cb.swap(cb);
        }else{
            event_ctx.fiber = Fiber::GetThis();
            SYLAR_ASSERT2(event_ctx.fiber->getState() == Fiber::EXEC
                            ,"state="<<event_ctx.fiber->getState());
        }
        return 0;
    }

    // 删除事件
    bool IOManager::delEvent(int fd, Event event)
    {
        RWMutexType::ReadLock lock(mutex_);
        if((int)fdContexts_.size() <= fd)
        {
            return false;
        }
        FdContext* fd_ctx =  fdContexts_[fd];
        lock.unlock();
        
        FdContext::MutexType::Lock lock1(fd_ctx->mutex);
        if(SYLAR_UNLICKLY(!(fd_ctx->events & event)))
        {
            return false;
        }
        
        Event new_event = (Event)(fd_ctx->events & ~event);
        int op = new_event ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
        epoll_event epevent;
        epevent.events = EPOLLET | new_event;
        epevent.data.ptr  = fd_ctx;

        int rt = epoll_ctl(epfd_,op,fd,&epevent);
        if(rt)
        {
            SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << epfd_
            <<"," << op << "," << fd << "," << epevent.events
            <<"):"<< rt <<" ("<< errno << ") (" << strerror(errno)
                << ")";
            return false;
        }

        --pendingEventCount_;
        fd_ctx->events = new_event;
        FdContext::EventContext& event_ctx = fd_ctx->getContext(event);
        fd_ctx->resetContext(event_ctx);
        return true;
    }
    // 取消事件  =  删除 + 强制触发执行
    bool IOManager::cancelEvent(int fd, Event event)
    {
        RWMutexType::ReadLock lock3(mutex_);
        if((int)fdContexts_.size() <= fd)
        {
            return false;
        }
        FdContext* fd_ctx =  fdContexts_[fd];
        lock3.unlock();
        
        FdContext::MutexType::Lock lock(fd_ctx->mutex);
        if(SYLAR_UNLICKLY(!(fd_ctx->events & event)))
        {
            return false;
        }
        
        Event new_event = (Event)(fd_ctx->events & ~event);
        int op = new_event ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
        epoll_event epevent;
        epevent.events = EPOLLET | new_event;
        epevent.data.ptr  = fd_ctx;

        int rt = epoll_ctl(epfd_,op,fd,&epevent);
        if(rt)
        {
            SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << epfd_
            <<"," << op << "," << fd << "," << epevent.events
            <<"):"<< rt <<" ("<< errno << ") (" << strerror(errno)
                << ")";
            return false;
        }

        fd_ctx->triggerEvent(event);
        --pendingEventCount_;
        return true;
    }
    // 取消所有事件
    bool IOManager::cancelAll(int fd)
    {
        RWMutexType::ReadLock lock(mutex_);
        if((int)fdContexts_.size() <= fd)
        {
            return false;
        }
        FdContext* fd_ctx =  fdContexts_[fd];
        lock.unlock();
        
        FdContext::MutexType::Lock lock4(fd_ctx->mutex);
        if(!(fd_ctx->events))
        {
            return false;
        }
        
        int op = EPOLL_CTL_DEL;
        epoll_event epevent;
        epevent.events = 0;
        epevent.data.ptr  = fd_ctx;

        int rt = epoll_ctl(epfd_,op,fd,&epevent);
        if(rt)
        {
            SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << epfd_
            <<"," << op << "," << fd << "," << epevent.events
            <<"):"<< rt <<" ("<< errno << ") (" << strerror(errno)
                << ")";
                return false;
        }
        if(fd_ctx->events & READ)
        {
            fd_ctx->triggerEvent(READ);
            --pendingEventCount_;
        }
        if(fd_ctx->events & WRITE)
        {
            fd_ctx->triggerEvent(WRITE);
            --pendingEventCount_;
        }

        SYLAR_ASSERT(fd_ctx->events == 0);
        return true;
    }

    // 获取当前的IOManager
    IOManager *IOManager::GetThis()
    {
        return dynamic_cast<IOManager*>(Scheduler::GetThis());
    }

    //没有空闲线程就退出（idle线程） 有就给epoll发送一个消息 唤醒阻塞到epoll_wait的线程
    void IOManager::tickle()
    {
        // SYLAR_LOG_DEBUG(g_logger) << "IOManager::tickle()";
        //没有可以直接使用的线程
        if(!hasThreadIdle()){
            return;
        }
        int rt = write(ticklefds_[1],"T",1);
        SYLAR_ASSERT(rt == 1);
    }

    //未处理的消息数量为0 | 定时器中没有定时任务 | 调度器处于停止状态
    bool IOManager::stopping()
    {
        uint64_t timeout = 0;
        return stopping(timeout);
    }

    //未处理的消息数量为0 | 定时器中没有定时任务 | 调度器处于停止状态
    bool IOManager::stopping(uint64_t& timeout)
    {
        timeout = getNextTimer();
        return timeout == ~0ull
            && pendingEventCount_ == 0
            && Scheduler::stopping();
    }
    IOManager::FdContext* IOManager::getFdContext(int fd) {
        RWMutexType::ReadLock lock(mutex_);
        if(fdContexts_[fd] && fdContexts_[fd]->fd) {
            return fdContexts_[fd];
        }
        return nullptr;
    }
    //////////////////maxCore
    //执行函数  -> mainFiber
    void IOManager::idle()
    {
        // Scheduler::idle();
        // SYLAR_LOG_DEBUG(g_logger) << "IOManager::idle()";
        const uint64_t MAX_EVENTS = 256;
        epoll_event* events = new epoll_event[MAX_EVENTS]();
        std::shared_ptr<epoll_event> shared_events(events, [](epoll_event *ptr){ delete[] ptr; });

        while (true)
        {
            uint64_t next_timeout = 0;
                if(SYLAR_UNLICKLY(stopping(next_timeout))){//严重错误
                    SYLAR_LOG_INFO(g_logger) << "name=" <<getName()
                                        << " idle stopping exit";
                    break;
                }     

            int rt = 0;
            do
            { // epoll 是 ms 级别
                static const int MAX_TIMEOUT = 5000;
                if(next_timeout != ~0ull){
                    next_timeout = (int)next_timeout > MAX_TIMEOUT ? MAX_TIMEOUT : next_timeout;
                }else{
                    next_timeout = MAX_TIMEOUT;
                }
                rt = epoll_wait(epfd_, events, MAX_EVENTS, (int)next_timeout);

                if (rt < 0 && errno == EINTR){
                }else{
                    break;
                }
            } while (true);

            //进入下一轮循环就是空的
            std::vector<std::function<void()>> cbs;
            listExpiredCb(cbs);
            
            if(!cbs.empty()){//swap的原因在这里
                schedule(cbs.begin(),cbs.end());
                cbs.clear();
            }
           
            for (int i = 0; i < rt; i++)
            {
                epoll_event& event = events[i];
                if (event.data.fd == ticklefds_[0])
                {
                    uint8_t dummy[256];
                    // 边沿触发只触发一次，读到没有数据
                    while (read(ticklefds_[0], &dummy, sizeof(dummy)) > 0); 
                    //不加continue 每次通知的时候也会向下进行读取fd，进行到fd_ctx->mutex出现
                    // segmentation fault(访问一个无效的地址 访问未初始化的指针或者被赋值为 NULL 的指针) 
                    continue;  
                }

                FdContext *fd_ctx = (FdContext*)event.data.ptr;
                //FdContext *fd_ctx = (FdContext *)fdContexts_[7];
                FdContext::MutexType::Lock lock(fd_ctx->mutex);
                if (event.events & (EPOLLERR | EPOLLHUP))
                {
                    // 错误或者中断
                    event.events |= (EPOLLIN | EPOLLOUT) & fd_ctx->events;
                }

                int real_events = NONE;
                if (event.events & EPOLLIN)
                {
                    real_events |= READ;
                }
                if (event.events & EPOLLOUT)
                {
                    real_events |= WRITE;
                }

                if ((fd_ctx->events & real_events) == NONE)
                {
                    continue;
                }

                // 剩余的事件
                int left_events = (fd_ctx->events & ~real_events);
                int op = left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
                event.events = EPOLLET | left_events;

                int rt2 = epoll_ctl(epfd_, op, fd_ctx->fd, &event);
                if (rt2)
                {
                    SYLAR_LOG_ERROR(g_logger) << "epoll_ctl(" << epfd_
                                              << "," << op << "," << fd_ctx->fd << "," << event.events
                                              << "):" << rt << " (" << errno << ") (" << strerror(errno)
                                              << ")";
                    continue;
                }

                if (real_events & READ)
                {
                    fd_ctx->triggerEvent(READ);
                    --pendingEventCount_;
                }
                if (real_events & WRITE)
                {
                    fd_ctx->triggerEvent(WRITE);
                    --pendingEventCount_;
                }
            }
            Fiber::ptr cur = Fiber::GetThis();
            auto raw_ptr = cur.get();
            cur.reset();
            
            raw_ptr->swapOut();
            // sylar::Fiber::YeildToReady();
        }
    }

    void IOManager::onTimerInsertAtFront()
    {   
        //给epoll发送一个通知
        tickle();
    }

}