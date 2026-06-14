#include"sylar/fiber.h"
#include"sylar/log.h"
#include"sylar/config.h"
#include"sylar/scheduler.h"
namespace sylar
{
    // static sylar::Logger::ptr g_logger = SYLAR_LOG_ROOT();
    /**
     * 非对称协程
     * 一个进程有一个 main协程 n个主协程（n个线程） m个子协程
     * 单一线程的主协程？ 多线程的主协程和次主协程 Scheduler可以创建几个
     * 理解s_fiber count / id   id是赋值   count是计数
     */
    //怎么保证全局静态变量的初始化顺序优先于成员静态函数
    static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");
    static std::atomic<uint64_t> s_fiber_id {0};            // 0 - 主  other - 子
    static std::atomic<uint64_t> s_fiber_count {0};         //
    
    static thread_local Fiber* t_fiber = nullptr;           //当前线程指针
    static thread_local Fiber::ptr t_threadFiber = nullptr; //每一个线程的主协程
    
    static ConfigVar<uint32_t>::ptr g_fiber_static_size =
        Config::Lookup<uint32_t>("fiber.stack_size",1024*1024,"fiber stack size");
    
    //空间开辟器
    class MallocStackAllocator
    {
        public:
        static void* Alloc(size_t size)
        {
            return malloc(size);
        }
        
        static void Dealloc(void* vp,size_t size)
        {
            free(vp);
        }
    };
    using StackAllocator = MallocStackAllocator;
    /**
     * getcontext   设置ctx的上下文
     * makecontext  设置ctx恢复后的执行函数
     * setcontext   恢复ctx的上下文
     * swapcontext
     */
    //创建主协程 不设置mainfunc
    Fiber::Fiber()
    {
        state_ = EXEC;
        SetThis(this);

        if(getcontext(&ctx_))
        {
            SYLAR_ASSERT2(false,"getcontex");
        }
        ++s_fiber_count;

        // SYLAR_LOG_DEBUG(g_logger)<<"main_fiber id: " << id_;
    }
    // 创建子协程 设置mainfunc
    /**
     * 为什么需要alloc stack
     * 协程在执行func 的时候如果func的变量需要很大的内存，都存在栈上肯定会溢出
     * ，我们创建一个"栈"空间存储这些变量
     * 开辟空间  -  获取上下文 -  执行真正的func
     */
    Fiber::Fiber(std::function<void()> cb, size_t stacksize,bool use_caller)
        :id_(++s_fiber_id)
        ,cb_(cb){
        ++s_fiber_count;
        stackSize_ = (stacksize > 0) ? stacksize : g_fiber_static_size->getValue();

        stack_ = sylar::StackAllocator::Alloc(stackSize_);
        //调用 getcontext(&ctx)，这个函数会把当前的上下文信息（像程序计数器、寄存器值、栈指针等）
        //保存到 ctx 变量中。这里的上下文就是程序执行到 getcontext 这一行时的状态
        if(getcontext(&ctx_))
        {
            SYLAR_ASSERT2(false,"getcontex");
        }
        //关联的上下文  函数结束以后自动回到指向的uc_link
        ctx_.uc_link = nullptr;
        //开辟的栈空间
        ctx_.uc_stack.ss_sp = stack_;
        ctx_.uc_stack.ss_size = stackSize_;
        //修改调用setcontex（ctx_）执行的路径
        if(!use_caller){
            makecontext(&ctx_,&Fiber::MainFunc,0);
        }else{//use_caller
            makecontext(&ctx_,&Fiber::CallerMainFunc,0);
        }
        // SYLAR_LOG_DEBUG(g_logger)<<"fiber id: " << id_;
    }
    // 析构函数
    Fiber::~Fiber()
    {
        --s_fiber_count;
        if(stack_)
        {
            //有stack说明是子协程
            SYLAR_ASSERT( state_ == TERM
                        || state_ == INIT
                        || state_ == EXCEPT);
            StackAllocator::Dealloc(stack_,stackSize_);
        }else{//主协程
            //主协程没有cb 
            SYLAR_ASSERT(!cb_);
            SYLAR_ASSERT(state_ == EXEC);

            Fiber* cur = t_fiber;
            if(cur == this){//防止内存泄漏
                SetThis(nullptr);//出作用域自动销毁
            }
        }
        SYLAR_LOG_DEBUG(g_logger)<<"Fiber::~Fiber() id=" << id_
                                    << " total=" << s_fiber_count;
    }
    // 重置cb_(充分利用内存 协程执行完 内存内释放 继续基于这个内存创建一个新的协程)
    void Fiber::reset(std::function<void()> cb)
    {
        SYLAR_ASSERT(stack_);
        SYLAR_ASSERT(state_ == TERM 
                    || state_ == INIT
                    || state_ == EXCEPT);
        cb_ = cb;
        if(getcontext(&ctx_))
        {
            SYLAR_ASSERT2(false,"getcontext");
        }

        //关联的上下文  函数结束以后自动回到指向的uc_link
        ctx_.uc_link = nullptr;
        //开辟的栈空间
        ctx_.uc_stack.ss_sp = stack_;
        ctx_.uc_stack.ss_size = stackSize_;
        //修改调用setcontex（ctx_）执行的路径
        makecontext(&ctx_,&Fiber::MainFunc,0);
        state_ = INIT;
    }

    // 子协程 切换到  单一线程的主协程
    void Fiber::swapOut()
    {
        SetThis(Scheduler::GetMainFiber());
        if(swapcontext(&ctx_,&Scheduler::GetMainFiber()->ctx_)){
            SYLAR_ASSERT2(false,"swapcontext");
        }
    }
    // 每个线程的主协程 -> 子协程
    void Fiber::swapIn()
    {
        SetThis(this);
        SYLAR_ASSERT(state_ != EXEC);
        //构造函数中已经把ctx返回的执行内容设置成了MainFunc
        state_ = EXEC;
        //swapcontext 函数在进入 ucp 所指向的上下文执行结束后，
        //通常会恢复 oucp 所指向的上下文继续执行，前提是没有其他的上下文切换操作介入。
        if(swapcontext(&Scheduler::GetMainFiber()->ctx_,&ctx_))
        {
            SYLAR_ASSERT2(false,"swapcontext");
        }
    }
     //threadFiber  ->  当前创建的协程 (适用于user_caller)
     void Fiber::call()
     {
        SetThis(this);
        state_ = EXEC;
        if(swapcontext(&t_threadFiber->ctx_,&ctx_))
        {
            SYLAR_ASSERT2(false,"swapcontext");
        }
     }
     // 当前创建的协程 -> threadFiber(适用于user_caller)
     void Fiber::back()
     {
        SetThis(t_threadFiber.get());
        if(swapcontext(&ctx_, &t_threadFiber->ctx_)) {
        SYLAR_ASSERT2(false, "swapcontext");
    }
     }

    // 协程的cb执行函数
    void Fiber::MainFunc() 
    {
        Fiber::ptr cur = GetThis();
        SYLAR_ASSERT(cur);
         //SYLAR_ASSERT(!(cur == t_threadFiber));
        try{
            cur->cb_();
            cur->cb_ = nullptr;
            cur->state_ = TERM;
        }catch(std::exception& ex){
            cur->state_ = EXCEPT;
            SYLAR_LOG_ERROR(g_logger) << "Fiber Except: "<<ex.what()
            << " fiber_id=" << cur->getId()
            <<std::endl
            <<sylar::BacktraceToString();
        }catch(...){
            cur->state_ = EXCEPT;
            SYLAR_LOG_ERROR(g_logger) << "Fiber Except"
            << " fiber_id=" << cur->getId()
            <<std::endl
            <<sylar::BacktraceToString();
        } 

        auto f = cur.get();
        cur.reset();
        f->swapOut();

        SYLAR_ASSERT2(false,"never reach fiber_id = " + std::to_string(cur->getId()));
    }

    //user_caller类型的mainFunc
    void Fiber::CallerMainFunc()
    {
        Fiber::ptr cur = GetThis();
        SYLAR_ASSERT(cur);
         //SYLAR_ASSERT(!(cur == t_threadFiber));
        try{
            cur->cb_();
            cur->cb_ = nullptr;
            cur->state_ = TERM;
        }catch(std::exception& ex){
            cur->state_ = EXCEPT;
            SYLAR_LOG_ERROR(g_logger) << "Fiber Except: "<<ex.what()
            << " fiber_id=" << cur->getId()
            << std::endl
            << sylar::BacktraceToString();
        }catch(...){
            cur->state_ = EXCEPT;
            SYLAR_LOG_ERROR(g_logger) << "Fiber Except"
            << " fiber_id=" << cur->getId()
            << std::endl << sylar::BacktraceToString();
        }

        auto f = cur.get();
        cur.reset();
        f->back();
        SYLAR_ASSERT2(false,"never reach fiber_id = " + std::to_string(cur->getId()));
    }

    // 切换到主协程  并把子协程 设置为HOLD
    void Fiber::YeildToHold() 
    {
        Fiber::ptr cur = GetThis();
        SYLAR_ASSERT(cur->state_ == EXEC);
        // cur->state_ = HOLD;
        cur->swapOut();
    }
    // 切换到主协程  并把子协程 设置为READY
    void Fiber::YeildToReady() 
    {
        Fiber::ptr cur = GetThis();
        SYLAR_ASSERT(cur->state_ == EXEC);
        cur->state_ = READY;
        cur->swapOut();
    }

    uint64_t Fiber::GetFiberId()
    {
        if(t_fiber)
        {
            return t_fiber->getId();
        }
        return 0;
    }
    // 返回当前协程（首次则首次创建一个协程）
    Fiber::ptr Fiber::GetThis() 
    {
        //说明已经有主协程 有主协程就会进入if
        if(t_fiber)//只要是正常执行t_fiber默认认为没有问题
        {
            return t_fiber->shared_from_this();
        }
        //没有t_fiber  说明还没有创建 创建主协程
        Fiber::ptr main_fiber(new Fiber); //设置t_fiber
        SYLAR_ASSERT(t_fiber == main_fiber.get());
        t_threadFiber = main_fiber;
        return t_fiber->shared_from_this();
    }
    // 设置当前协程
    void Fiber::SetThis(Fiber* fiber) 
    {
        t_fiber = fiber;
    }

    // 返回协程的总数
    uint64_t Fiber::GetTotalCount() 
    {
        return s_fiber_count;
    }

}