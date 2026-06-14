#ifndef __SYLAR_IOMANAGER_H__
#define __SYLAR_IOMANAGER_H__
#include "sylar/scheduler.h"
#include "sylar/timer.h"
#include <sys/epoll.h>
#include <unistd.h>

namespace sylar
{

    class TimerManager;
    //IO调度器(epoll)(配合管道 实现 异步io)
    class IOManager:public Scheduler,public TimerManager
    {
    public:
        typedef std::shared_ptr<IOManager>ptr;
        typedef RWMutex RWMutexType;
        enum Event
        {
            NONE  = 0x0,    //空事件
            READ  = 0x1,    //读事件
            WRITE = 0x4,    //写事件
        };   
    private:
        struct FdContext
        {
            typedef Mutex MutexType;
            struct EventContext
            {
                Scheduler* scheduler = nullptr;     //事件执行的scheduler
                Fiber::ptr fiber;                   //事件的协程
                std::function<void()> cb;           //事件的回调函数
            };
            
            EventContext& getContext(Event event);
            void resetContext(EventContext& ctx);
            void triggerEvent(Event ctx);

            EventContext read;  //读事件
            EventContext write; //写事件
            int fd = 0;         //事件关联的句柄
            Event events = NONE;//已注册的事件
            MutexType mutex;    //互斥锁
        };
    public:
        //初始化  配置管道
        IOManager(size_t threads = 1,bool use_caller = true, const std::string& name = "");
        //回收资源
        ~IOManager();
        //
        //1 sucess 0 retry -1 error
        //增加事件
        int addEvent(int fd,Event event,std::function<void()>cb = nullptr);
        //删除事件
        bool delEvent(int fd,Event event);
        //取消事件 删除前进行一次触发 
        // (比如一个fd ,想取消READ 那么在真正取消之前会进行一次执行)
        bool cancelEvent(int fd,Event event);
        //取消所有事件 
        //(比如一个fd ,想取消READ WRITE ，只要存在这个事件那么在真正取消之前会进行一次执行)
        bool cancelAll(int fd);
        
        //获取当前的IOManager
        static IOManager* GetThis();
    protected:
    public:
        void tickle()override;
        bool stopping()override;
        void onTimerInsertAtFront()override;
        void idle()override;
        //重置FdContext的大小
        void ContextResize(size_t size);
        bool stopping(uint64_t& timeout);
        FdContext* getFdContext(int fd);
    private:
        int epfd_ = 0;      //epoll句柄
        int ticklefds_[2];  //通知管道
        std::atomic<size_t> pendingEventCount_ = {0};   //未处理的消息数量
        RWMutexType mutex_; //读写锁
        std::vector<FdContext*> fdContexts_;    //存储句柄和对应的事件
    }; 
    /**
     * sylar采用的是双向管道
     * 优点实时处理 异步请求和消息队列的请求
     */
}

#endif