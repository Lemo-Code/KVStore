#include "sylar/scheduler.h"
#include "sylar/log.h"
#include "sylar/hook.h"

/**
 * 思路梳理
 * 一个调度器对应一个主协程 n-1个子协程
 * 或者一个调度器对应n个子协程
 * 这里的主协程是只的调度器的主协程 Fiber.cc中的t_threadFiber指的是真正意义main的主协程
 */
/**
 * 是user_caller就创建协程去执行scheduler::run
 * 不是uer_caller就创建新线程 在新线程去执行run
 */
namespace sylar
{
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

    //协程调度器的指针（thread_local会在每一个线程为调度器创建一个副本）
    static thread_local Scheduler* t_scheduler = nullptr;
    // 每个线程的主协程
    static thread_local Fiber* t_scheduler_fiber = nullptr;

    //{threads 线程数}  {use_caller [true 主线程 + n-1线程]  [false n个线程]}
    //主线程不会以线程的形式执行run方法  而是创建一个子协程去执行run方法
    //其他线程的入口函数是run
    Scheduler::Scheduler(size_t threads, bool use_caller, const std::string &name)
        : name_(name){
        set_hook_enable(true);
        SYLAR_ASSERT(threads > 0);
        SYLAR_LOG_DEBUG(g_logger) <<" Scheduler::Scheduler()";

        if(use_caller){
            //为线程创建一个主协程
            sylar::Fiber::GetThis();
            --threads;

            // 说明目前的操作并不是在作为调度任务执行的（并不是在其他调度器作为任务执行）
            SYLAR_ASSERT(GetThis() == nullptr);
            t_scheduler = this;

            // 主线程的主协程，入口函数是run  true是为了和当前线程保持一致（主线程和其他线程的调度方法有区别）
            rootFiber_.reset(new Fiber(std::bind(&Scheduler::run, this), 0, true));
            sylar::Thread::SetName(name_);

            //设置当前线程的主协程
            t_scheduler_fiber = rootFiber_.get();
            rootThread_ = sylar::GetThreadId();
            threadIds_.push_back(rootThread_);
        } else {
            rootThread_ = -1;
        }
        threadCount_ = threads;
    }

    // 启动线程池
    void Scheduler::start()
    {
        MutexType::Lock lock(mutex_);
        // SYLAR_LOG_INFO(g_logger) << this << " Scheduler::start() ";
        if (!stopping_){
            return;
        }
        stopping_ = false;
        SYLAR_ASSERT(threads_.empty());

        threads_.resize(threadCount_);
        for (size_t i = 0; i < threadCount_; i++){
            threads_[i].reset(new Thread(std::bind(&Scheduler::run, this)
                            , name_ + "_" + std::to_string(i)));
            threadIds_.push_back(threads_[i]->getId());
        }
        lock.unlock();
    }

    // 手动关闭线程池
    void Scheduler::stop()
    {
        autoStop_ = true;
        if (rootFiber_ 
                && threadCount_ == 0 
                && (rootFiber_->getState() == Fiber::INIT 
                    || rootFiber_->getState() == Fiber::TERM)){
            // SYLAR_LOG_INFO(g_logger) << this << " Scheduler::stop()";
            stopping_ = true;
            //实现多类型结束（子类和父类的结束方案不同）
            //子类可能需要对某些内容做个性化处理 或者 判别结束的方案也不同
            if (stopping()){
                return;
            }
        }

        //处理主线程
        if (rootThread_ != -1){
            SYLAR_ASSERT(GetThis() == this);
        } else {
            // 非use_caller模式没有设置t_scheduler(nullptr)
            SYLAR_ASSERT(GetThis() != this);
        }

        stopping_ = true;
        for(size_t i = 0; i < threadCount_; i++){
            tickle(); // 没明白
        }
        if(rootFiber_){
            tickle();
        }

        if(rootFiber_){
            if(!stopping()){
                rootFiber_->call();
            }
        }
        // 不直接回收资源的原因
        // swap交换速度快  /  局部变量线程安全
        std::vector<Thread::ptr> thrs;
        {
            MutexType::Lock lock(mutex_);
            thrs.swap(threads_);
        }
        for (auto &i : thrs)
        {
            i->join();
        }
        SYLAR_LOG_INFO(g_logger) << this << " Scheduler::stop()";
    }

    // t_schduler存储当前线程的调度器指针
    //当在A线程 使用t_scheduler就是指向的当前线程的调度器 不会和其他线程调度器发生冲突
    void Scheduler::setThis(){
        t_scheduler = this;
    }
    
    /*取任务:用ft遍历队列取任务  
     *    若遍历的当前任务为指定线程任务 跳过该任务 设置need_tickle为true
     *    若当前任务为fiber类型的任务，就判断状态，若是EXEC就跳过
     *    其他任务都符合条件，取出任务并退出遍历执行下一步
     *通知其他阻塞线程:
     *    若need_tickle则通知
     *执行任务
     *    fiber任务
     *        状态是TERM | EXCEP，进入下一轮循环
     *        否则执行任务
     *            执行完是READY状态：重新加入任务，重置ft，进入下一轮循环
     *            执行完是TERM | EXCEP 就重置状态，并重置ft
     *    cb任务：先分配一个协程 并执行任务，并把ft重置
     *            执行完若是READY状态 就加入队列
     *            执行完是TERM | EXCET状态就把协程设为空
     *            否则就设置fiber状态为HOLD，重置协程
     *    等待其他任务的协程
     *            若是该idle协程状态是TERM EXCEPT 就退出整个循环
     *                否则就执行 ，执行完若不是TERM || EXCEPT 就设置为HOLD 进入下一轮循环
    */
    void Scheduler::run()
    {
        // SYLAR_LOG_DEBUG(g_logger) << "Scheduler::run()";
        set_hook_enable(true);//影响hook !!!
        setThis();
        //创建当前线程的主协程
        if (sylar::GetThreadId() != rootThread_){
            t_scheduler_fiber = Fiber::GetThis().get();
        }

        // 等待任务的协程
        Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle, this)));
        // 具体的执行任务的协程
        Fiber::ptr cb_fiber;
        
        //ft用来取任务(一次循环取一个)
        FiberAndThread ft;
        
        while (true){
            //（1）取任务
            ft.reset();
            bool tickle_me = false;
            bool is_active = false;

            {
                MutexType::Lock lock(mutex_);
                auto it = fibers_.begin();
                while (it != fibers_.end()){
                    if (it->thread != -1 && it->thread != sylar::GetThreadId()){
                        ++it;
                        tickle_me = true; // 发起信号
                        continue;
                    }

                    SYLAR_ASSERT(it->fiber || it->cb);
                    if (it->fiber && it->fiber->getState() == Fiber::EXEC){
                        ++it;
                        continue;
                    }
                    ft = *it;
                    fibers_.erase(it++);
                    //一取出任务就加一
                    ++activeThreadCount_;
                    is_active = true;
                    break;
                }
                tickle_me |= it != fibers_.end();
            }

            // (2) 通知线程
            if (tickle_me){
                tickle();
            }

            // (3) 执行任务
            if (ft.fiber && (ft.fiber->getState() != Fiber::TERM 
                        && ft.fiber->getState() != Fiber::EXCEPT)){
                //进入任务
                ft.fiber->swapIn(); 
                --activeThreadCount_;

                if (ft.fiber->getState() == Fiber::READY){
                    schedule(ft.fiber);
                }else if (ft.fiber->getState() != Fiber::TERM 
                        && ft.fiber->getState() != Fiber::EXCEPT){
                    ft.fiber->state_ = Fiber::HOLD;
                }
                ft.reset();
            } else if(ft.cb) {//cb
                if(cb_fiber){
                    cb_fiber->reset(ft.cb);
                } else {
                    cb_fiber.reset(new Fiber(ft.cb));
                }
                ft.reset();
                //进入任务
                cb_fiber->swapIn();
                --activeThreadCount_;

                if (cb_fiber->getState() == Fiber::READY){ // ready状态就加入队列
                    schedule(cb_fiber);
                    cb_fiber.reset();
                } else if(cb_fiber->getState() == Fiber::EXCEPT 
                    || cb_fiber->getState() == Fiber::TERM){ 
                    // 结束状态就重置
                    cb_fiber->reset(nullptr);
                }else {   
                     // 没有结束 //理解Hold状态的真正内涵
                    cb_fiber->state_ = Fiber::HOLD;
                    cb_fiber.reset();
                }
            } else {//idle
                if(is_active){
                    --activeThreadCount_;
                    continue;
                }
                if (idle_fiber->getState() == Fiber::TERM
                        /*|| idle_fiber->getState() == Fiber::EXCEPT*/){
                    SYLAR_LOG_INFO(g_logger) << "Scheduler::run() idle fiber term";
                    break;//退出循环的条件
                }
                ++idleThreadCount_;
                //进入等待任务
                idle_fiber->swapIn();
                --idleThreadCount_;
                if (idle_fiber->getState() != Fiber::TERM 
                        && idle_fiber->getState() != Fiber::EXCEPT){
                    idle_fiber->state_ = Fiber::HOLD;
                }
            }
        }
    }


    // 是否达成结束条件（子类还能根据自身对变量提供一些特例化处理）
    bool Scheduler::stopping()
    {
        MutexType::Lock lock(mutex_);
        return autoStop_ 
                && stopping_ 
                && fibers_.empty() 
                && activeThreadCount_ == 0;
    }

    // 通知线程（唤醒线程执行任务）
    // 执行时机： 
    //1- schedule()放任务的时候会tickle(放任务)
    //2- run方法中取任务但不是自己所在的线程（指定线程的任务）会通知其他线程取（时间会比较长）
    //3- run方法中成功取出任务
    void Scheduler::tickle()
    {
        SYLAR_LOG_INFO(g_logger) << "Scheduler::tickle()";
    }

    // 等待任务(在任务队列没有任务的时候)
    // 只要协程调度没有结束，我就让每一个线程执行完idle都回去重新进入循环。
    // 直到手动停止并且没有任务了才彻底从run中退出(只要我idle设置状态，就不可能退出run) 
    void Scheduler::idle()
    {
        // SYLAR_LOG_INFO(g_logger) << "Scheduler::idle()";
        while(!stopping()){//while(循环入口)
            sylar::Fiber::YeildToHold();
        }
    }

    // 父类对象析构(置空线程资源)
    Scheduler::~Scheduler()
    {
        SYLAR_LOG_DEBUG(g_logger) << "Scheduler::~Scheduler()";
        SYLAR_ASSERT(stopping_);
        // 说明只是回收t_scheduler的资源
        if (GetThis() == this)
        {
            t_scheduler = nullptr;
        }
    }

    // 获得当前线程的调度器
    Scheduler *Scheduler::GetThis()
    {
        return t_scheduler;
    }
    // 返回的是主线程的主协程
    Fiber *Scheduler::GetMainFiber()
    {
        return t_scheduler_fiber;
    }

    std::ostream& Scheduler::dump(std::ostream& os) {
        os << "[Scheduler name=" << name_
        << " size=" << threadCount_
        << " active_count=" << activeThreadCount_
        << " idle_count=" << idleThreadCount_
        << " stopping=" << stopping_
        << " ]" << std::endl << "    ";
        for(size_t i = 0; i < threadIds_.size(); ++i) {
            if(i) {
                os << ", ";
            }
            os << threadIds_[i];
        }
        return os;
    }
    void Scheduler::switchTo(int thread) {
        SYLAR_ASSERT(Scheduler::GetThis() != nullptr);
        if(Scheduler::GetThis() == this) {
            if(thread == -1 || thread == sylar::GetThreadId()) {
                return;
            }
        }
        schedule(Fiber::GetThis(), thread);
        Fiber::YeildToHold();
    }

    SchedulerSwitcher::SchedulerSwitcher(Scheduler* target) {
        m_caller = Scheduler::GetThis();
        if(target) {
            target->switchTo();
        }
    }

    SchedulerSwitcher::~SchedulerSwitcher() {
        if(m_caller) {
            m_caller->switchTo();
        }
    }


}