#ifndef __SYLAR_TIMER_H__
#define __SYLAR_TIMER_H__
#include "sylar/mutex.h"
#include <memory>
#include <functional>
#include <set>
#include <vector>
/**
 * 试试把#include<config>换成class 预定义
 */
/**
  * 添加定时器
  * 取消定时器
  * 获取当前的定时器触发离现在的时间差
  * 返回当前需要触发的定时器
  */

namespace sylar
{
    // class IOManager;
    class TimerManager;
    class Timer:public std::enable_shared_from_this<Timer>
    {
    friend class TimerManager;
    public:
        typedef std::shared_ptr<Timer>ptr;
        bool cancel();
        bool refresh();
        bool reset(uint64_t ms,bool from_now);
    private:    
        Timer(uint64_t ms,std::function<void()>cb,
                bool recurring,TimerManager* manager);
        Timer(uint64_t next);
    private:
        uint64_t ms_ = 0;          //执行周期
        std::function<void()> cb_ = nullptr; //
        bool recurring_ = false;   //是否循环定时器
        TimerManager* manager_ = nullptr;    //
        uint64_t next_ = 0;        //精确的执行时间
        private:
        struct Comparator
        {
            bool operator()(const Timer::ptr& lhs,const Timer::ptr& rhs)const;
        };
    };

    //IOManager继承获得
    class TimerManager
    {
    friend class Timer;
    public:
        typedef RWMutex RWMutexType;
        TimerManager();
        virtual ~TimerManager();
        Timer::ptr addTimer(uint64_t ms,std::function<void()>cb,
                                bool recurring = false);
        Timer::ptr addConditionTimer(uint64_t ms,std::function<void()>cb,
                                    std::weak_ptr<void>weak_cond,
                                    bool recurring = false);
        uint64_t getNextTimer();
    protected:
        virtual void onTimerInsertAtFront() = 0;
        void listExpiredCb(std::vector<std::function<void()> >&cbs);
        void addTimer(Timer::ptr val,RWMutexType::WriteLock& lock);
        bool hasTimer();
    private:
        bool detectClockRollover(uint64_t now_ms);
    private:
        RWMutexType mutex_;
        std::set<Timer::ptr,Timer::Comparator>timers_;
        bool ticked_ = false;
        uint64_t previouseTime_ = 0;
    };
}

#endif