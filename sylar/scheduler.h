#ifndef __SYLAR_SCHEDULER_H__
#define __SYLAR_SCHEDULER_H__
#include "sylar/thread.h"
#include "sylar/fiber.h"
#include "sylar/mutex.h"

#include <functional>
#include <memory>
#include <vector>



namespace sylar
{

/**
 *  协程状态的设置时机
 * 
 */


// 协程调度器
class Scheduler
{
public:
    typedef std::shared_ptr<Scheduler> ptr;
    typedef sylar::Mutex MutexType;

    //{threads 线程数}  {use_caller [true 主线程 + n-1线程]  [false n个线程]}
    Scheduler(size_t threads = 1, bool use_caller = true, const std::string& name = "");
    // 置空资源
    virtual ~Scheduler();
    // 获得调度器(线程池)名字
    const std::string &getName() const { return name_; }
    // 是否有空闲的线程
    bool hasThreadIdle() { return idleThreadCount_ > 0; }

public:
    // 获得当前线程下的协程调度器的主协程
    static Fiber* GetMainFiber();
    // 获得当前线程的调度器
    static Scheduler* GetThis();
    // 启动线程池
    void start();
    // 关闭线程池
    void stop();
    void switchTo(int thread = -1);
    std::ostream& dump(std::ostream& os);
protected:
    // 等待任务(在没有任务的时候)
    virtual void idle();
    // 是否达成结束条件（子类还能根据自身对变量提供一些特例化处理）
    virtual bool stopping();
    // 调度器的主函数（线程池进入的函数）
    // 流程：取任务 -> 观察是否需要通知其他线程 -> 根据类型以不同方式处理任务
    void run();
    // 通知线程（唤醒线程执行任务）
    virtual void tickle();
    // 设置调度器指针指向当前线程的调度器
    void setThis();
public:
    // 用户调用的存放任务的方法  
    // 功能:
    // 若在放任务时，任务队列为空，则tickle唤醒一下沉睡的线程
    // 可指定任务在具体的线程执行(非-1) 否则随机分配
    template <class FiberOrCb>
    void schedule(FiberOrCb fc, int thread = -1){
        bool need_tickle = false;
        {
            MutexType::Lock lock(mutex_);
            need_tickle = scheduleNoLock(fc, thread);
        }
        if (need_tickle){
            tickle();
        }
    }

    // 多个任务放入任务队列
    // 当初始任务队列为空时候才会唤醒线程
    // 此方法 存放任务同时会交换所有权
    template <class InputIterator>
    void schedule(InputIterator begin, InputIterator end){
        bool need_tickle = false;
        {
            MutexType::Lock lock(mutex_);
            while (begin != end){
                need_tickle = scheduleNoLock(&*begin, -1) || need_tickle;
                begin++;
            }
        }
        if (need_tickle){
            tickle();
        }
    }
protected:
    //不加锁存放任务  
    //任务队列为空  线程陷入内核态 加入任务后，不管这个任务有没有，我都设置need_tickle
    template <class FiberOrCb>
    bool scheduleNoLock(FiberOrCb fc, int thread){
        bool need_tickle = fibers_.empty();
        FiberAndThread ft(fc, thread);
        if (ft.fiber || ft.cb){
            fibers_.push_back(ft);
        }
        return need_tickle;
    }
private:
    //存储任务对象（cb/fiber）
    struct FiberAndThread
    {
        Fiber::ptr fiber;           //fiber类型的任务
        std::function<void()> cb;   //cb类型的任务
        int thread;                 //任务所在的线程（提供一个指定xxx任务在xx线程执行的方案）
                                    // -1 不指定线程  非-1 则指定任务到具体的线程去执行
        // fiber是要执行分任务
        FiberAndThread(Fiber::ptr f, int thr)
            : fiber(f), thread(thr) {}

        // fiber是要执行的任务，并把f的所有权移交给fiber
        FiberAndThread(Fiber::ptr *f, int thr)
            : thread(thr){
            fiber.swap(*f); 
        }
        
        // cb存储要执行的任务
        FiberAndThread(std::function<void()> c, int thr)
            : cb(c), thread(thr) {}
        
        // cb存储要执行的任务，并把c的所有权移交给cb
        FiberAndThread(std::function<void()> *c, int thr)
            : thread(thr){
            cb.swap(*c);
        }
        
        // 默认构造函数 容器存储对象需要一个能初始化对象的构造函数
        FiberAndThread() : thread(-1) {}

        //重置任务对象
        void reset(){
            fiber = nullptr;
            cb = nullptr;
            thread = -1;
        }
    };

private:
    MutexType mutex_;                  // 互斥锁
    std::vector<Thread::ptr> threads_; // 线程数组
    std::list<FiberAndThread> fibers_; // 执行内容(fiber)
    Fiber::ptr rootFiber_;             // 主线程的子协程
    std::string name_;                 // 调度器的名称
protected:
    std::vector<int> threadIds_;               // 存储线程号
    size_t threadCount_ = 0;                   // 线程总数
    std::atomic<size_t> activeThreadCount_{0}; // 活跃的线程数目
    std::atomic<size_t> idleThreadCount_{0};   // 空闲的线程数目
    bool stopping_ = true;                     // 调度器处于未启动状态
    bool autoStop_ = false;                    // 是否手动停止调度器
    int rootThread_ = 0;                       // user_caller的线程号
};


class SchedulerSwitcher : public Noncopyable {
public:
    SchedulerSwitcher(Scheduler* target = nullptr);
    ~SchedulerSwitcher();
private:
    Scheduler* m_caller;
};


}

#endif