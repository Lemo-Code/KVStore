#include "sylar/timer.h"
#include "sylar/util.h"
namespace sylar
{
    Timer::Timer(uint64_t ms, std::function<void()> cb,
                 bool recurring, TimerManager *manager)
                :ms_(ms)
                ,cb_(cb)
                ,recurring_(recurring)
                ,manager_(manager){
                    // 下一次的执行时间
                    next_ = sylar::GetCurrentMS() + ms_;
                }

    Timer::Timer(uint64_t next)
        : next_(next){

    }

    //有就删除
    bool Timer::cancel()
    {
        TimerManager::RWMutexType::WriteLock lock(manager_->mutex_);
        if(cb_){
            cb_ = nullptr;
            auto it = manager_->timers_.find(shared_from_this());
            manager_->timers_.erase(it);
            return true;
        }
        return false;
    }
    //有就在timers中删除  重置时间  然后放回
    bool Timer::refresh()
    {
        TimerManager::RWMutexType::WriteLock lock(manager_->mutex_);
        if(!cb_){// 为啥没有删除操作?
            return false;
        }
        auto it = manager_->timers_.find(shared_from_this());
        if(it == manager_->timers_.end()){
            return false;
        }
        manager_->timers_.erase(it);
        next_ = sylar::GetCurrentMS() + ms_;
        manager_->timers_.insert(shared_from_this());
        return true;
    }

    //from_now -> 是否从现在开始计算时间
    bool Timer::reset(uint64_t ms, bool from_now)
    {
        if(ms == ms_ && !from_now){
            return true;
        }
        TimerManager::RWMutexType::WriteLock lock(manager_->mutex_);
        if(!cb_){// 为啥没有删除操作?
            return false;
        }
        auto it = manager_->timers_.find(shared_from_this());
        if(it == manager_->timers_.end()){
            return false;
        }
        manager_->timers_.erase(it);
        uint64_t start = 0;
        if(from_now){
            start = sylar::GetCurrentMS();
        }else{
            start = next_ - ms_;
        }
        ms_ = ms;
        next_ = start + ms_;
        manager_->addTimer(shared_from_this(), lock);

        return true;
    }
    bool Timer::Comparator::operator()(const Timer::ptr &lhs, const Timer::ptr &rhs) const
    {
        if(!lhs && !rhs)
        {
            return false;
        }
        if(!lhs)
        {
            return true;
        }
        if(!rhs)
        {
            return false;
        }
        if(lhs->next_ < rhs->next_)
        {
            return true;
        }
        if(rhs->next_ < lhs->next_)
        {
            return false;
        }
        return lhs.get() < rhs.get();
    }

    TimerManager::TimerManager()
    {
        previouseTime_ = sylar::GetCurrentMS();
    }
    TimerManager::~TimerManager()
    {

    }
    //onTimerInsertAtFront  ->  ?
    //ticked ->  ?   
    void TimerManager::addTimer(Timer::ptr val,RWMutexType::WriteLock& lock)
    {
        auto it = timers_.insert(val).first;
        bool at_front = (it == timers_.begin()) && !ticked_;
        if(at_front){
            ticked_ = true;
        }
        lock.unlock();
        if(at_front){//
            onTimerInsertAtFront();
        }
    }
    
    Timer::ptr TimerManager::addTimer(uint64_t ms, std::function<void()> cb,
                                      bool recurring)
    {
        Timer::ptr timer(new Timer(ms,cb,recurring,this));
        RWMutexType::WriteLock lock(mutex_);
        //insert返回的是pair<itrator,bool>
        addTimer(timer,lock);
        return timer;
    }

    static void OnTimer(std::weak_ptr<void>weak_cond,std::function<void()>cb){
        std::shared_ptr<void> tmp = weak_cond.lock();
        if(tmp){
            cb();
        }
    }

    //cb 是什么
    Timer::ptr TimerManager::addConditionTimer(uint64_t ms, std::function<void()> cb,
                                               std::weak_ptr<void> weak_cond,
                                               bool recurring )
    {
        return addTimer(ms,std::bind(&OnTimer,weak_cond,cb),recurring);
    }

    //调用getNextTimer 说明要重新进行epoll_wait了
    uint64_t TimerManager::getNextTimer()
    {
        RWMutexType::ReadLock lock(mutex_);
        ticked_ = false;
        if(timers_.empty()){
            return ~0ull;
        }

        const Timer::ptr& next = *timers_.begin();
        uint64_t now_ms = sylar::GetCurrentMS();
        if(now_ms >= next->next_){//当前时间 超过了
                return 0;
        }else{//返回剩余的时间
            return next->next_ - now_ms;
        }
    }

    //用expired取出符合条件的timer  并在timers_删除   遍历expired的timer把其中的cb放入cbs 
    void TimerManager::listExpiredCb(std::vector<std::function<void()> >&cbs)
    {
        uint64_t now_ms = sylar::GetCurrentMS();
        std::vector<Timer::ptr> expired;
        {
            RWMutexType::ReadLock lock(mutex_);
            if(timers_.empty()){//比较函数只用看next_
                return;
            }
        }
        RWMutexType::WriteLock lock(mutex_);
            if(timers_.empty()){//比较函数只用看next_
                return;
            }

        //时间没问题 并且 还未到执行时间
        bool rollver = detectClockRollover(now_ms);
        if(!rollver && (*timers_.begin())->next_ > now_ms){
            return;
        }
        
        Timer::ptr now_timer(new Timer(now_ms));
        //时间有问题全部清理掉  没问题就正常处理
        auto it = rollver ? timers_.end() : timers_.lower_bound(now_timer);
        while(it != timers_.end() && (*it)->next_ == now_ms){
            ++it;
        }
        //把next_ 小于等于 当前时间 的timr都放起来
        expired.insert(expired.begin(),timers_.begin(),it);
        timers_.erase(timers_.begin(),it);
        
        cbs.reserve(expired.size());

        for(auto& timer:expired){
            cbs.push_back(timer->cb_);
            if(timer->recurring_){
                timer->next_ = now_ms + timer->ms_; //抄错了
                timers_.insert(timer);
            }else{
                timer->cb_ = nullptr;
            }
        }
    }

    bool TimerManager::hasTimer()
    {
        RWMutexType::ReadLock lock(mutex_);
        return !timers_.empty();
    }
    //调了时间如何解决
    bool TimerManager::detectClockRollover(uint64_t now_ms)
    {
        bool rollover = false;
        if(now_ms < previouseTime_
            && now_ms < (previouseTime_ - 60 * 60 * 1000)){
                //时间有问题
                rollover = true;
            }    
        previouseTime_ = now_ms;
        return rollover;
    }
}