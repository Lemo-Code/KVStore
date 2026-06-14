#ifndef __SYLAR_FIBER_H__
#define __SYLAR_FIBER_H__
#include<memory>
#include<functional>
#include<ucontext.h>
#include<atomic>
#include"sylar/lexicalcast.h"
#include"sylar/macro.h"

namespace sylar {
/**
 * 协程类
 */
//继承了shared_from_this 会有什么使用的规定
class Fiber:public std::enable_shared_from_this<Fiber> {
public:
    friend class Scheduler;
    typedef std::shared_ptr<Fiber> ptr;
    //协程的状态
    enum State
    {
        INIT = 0,
        HOLD = 1,
        EXEC = 2,
        TERM = 3,
        READY = 4,
        EXCEPT = 5,
    };
private:
    // 创建主协程
    Fiber();
public:
    // 创建子协程 设置真正的执行函数cb_ -> 设置协程环境 -> 设置ctx的运行函数MainFunc ->在MainFunc中执行cb_   
    Fiber(std::function<void()> cb, size_t stacksize = 0,bool use_caller = false);
    // 析构函数
    ~Fiber();
    // 重置cb_
    void reset(std::function<void()> cb); //传参问题scheduler
    // 切换到主协程
    void swapOut();
    // 切换到子协程
    void swapIn();
    //
    void back();
    //特殊的swapin 当前协程 -》 目标执行协程
    void call();
    //返回协程号
    uint64_t getId()const{  return  id_; }
    // 返回协程的状态
    State getState()const{return state_; }
    // 设置协程的状态
    void setState(State state){state_ = state;}
public:
    // 协程的cb执行函数
    static void MainFunc();
    //user_caller类型的mainFunc
    static void CallerMainFunc();
    // 返回当前协程（首次则创建主协程）
    static Fiber::ptr GetThis();
    // 设置当前协程
    static void SetThis(Fiber* fiber);
    // 切换到主协程  并把子协程 设置为HOLD
    static void YeildToHold();
    // 切换到主协程  并把子协程 设置为READY
    static void YeildToReady();
    // 返回协程的总数
    static uint64_t GetTotalCount();
    // 静态获得fiberId
    static uint64_t GetFiberId();
private:
    ucontext_t ctx_;           // 协程上下文
    void *stack_ = nullptr;    // 栈空间首地址

    uint64_t id_ = 0;          // 协程号
    uint32_t stackSize_ = 0;   // 栈(堆)空间大小
    State state_ = INIT;       // 协程的状态
    std::function<void()> cb_; // 执行函数
};
}

#endif