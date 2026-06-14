#ifndef __SYLAR_MUTEX_H__
#define __SYLAR_MUTEX_H__

#include<pthread.h>
#include<atomic>

namespace sylar
{

//锁的自动化管理
template<class T>
struct ScopedLockImpl{
public:
    ScopedLockImpl(T& mutex)
        :mutex_(mutex){
        mutex_.lock();
        locked_ = true;
    };
    
    ~ScopedLockImpl(){
        unlock();
    }

    void lock(){
        if(!locked_){
            mutex_.lock();
            locked_ = true;
        }
    }

    void unlock(){
        if(locked_){
            mutex_.unlock();
            locked_ = false;
        }
    }

private:
    T& mutex_;
    bool locked_;
};

//互斥锁
class Mutex {
public:
    typedef ScopedLockImpl<Mutex> Lock;
    Mutex(){
        pthread_mutex_init(&lock_,nullptr);
    }
    ~Mutex(){
        pthread_mutex_destroy(&lock_);
    }

    void lock(){
        pthread_mutex_lock(&lock_);
    }

    void unlock(){
        pthread_mutex_unlock(&lock_);
    }
private:
    pthread_mutex_t lock_;
};


//读锁的自动化管理
template<class T>
struct ReadScopedLockImpl{
public:
    ReadScopedLockImpl(T& mutex)
        :mutex_(mutex){
        mutex_.rdlock();
        locked_ = true;
    }
    ~ReadScopedLockImpl(){
        unlock();
    }

    void unlock(){
        if(locked_){
            mutex_.unlock();
            locked_ = false;
        }
    }
    void lock(){
        if(!locked_){
            mutex_.rdlock();
            locked_ = true;
        }
    }
private:
    T& mutex_;
    bool locked_ = false;
};

//写锁的自动化管理
template<class T>
struct WriteScopedLockImpl{
public:
    WriteScopedLockImpl(T& mutex)
        :mutex_(mutex){
        mutex_.wrlock();
        locked_ = true;
    }
    ~WriteScopedLockImpl(){
        unlock();
    }

    void unlock(){
        if(locked_){
            mutex_.unlock();
            locked_ = false;
        }
    }
    void lock(){
        if(!locked_){
            mutex_.wrlock();
            locked_ = true;
        }
    }
private:
    T& mutex_;
    bool locked_ = false;
};

//读写锁
class RWMutex {
public:
    typedef ReadScopedLockImpl<RWMutex> ReadLock;
    typedef ReadScopedLockImpl<RWMutex> WriteLock;

    RWMutex(){
        pthread_rwlock_init(&lock_,nullptr);
    }
    ~RWMutex(){
        pthread_rwlock_destroy(&lock_);
    }

    void rdlock(){
        pthread_rwlock_rdlock(&lock_);
    }
    
    void wrlock(){
        pthread_rwlock_wrlock(&lock_);
    }

    void unlock(){
        pthread_rwlock_unlock(&lock_);
    }
private:
    pthread_rwlock_t lock_;
};

//空读写锁
class NullRWMutex{
public:
    typedef ReadScopedLockImpl<NullRWMutex> ReadLock;
    typedef WriteScopedLockImpl<NullRWMutex> WriteLock;

    NullRWMutex(){}
    ~NullRWMutex(){}

    void rdlock(){}
    void wrlock(){}
    void unlock(){}
};

//空锁
class NullMutex{
public:
    typedef ScopedLockImpl<Mutex> Lock;
    NullMutex(){}
    ~NullMutex(){}
    
    void lock();
    void unlock();
};


//自旋锁
class Spinlock{
public:
    typedef ScopedLockImpl<Spinlock> Lock;
    Spinlock(){
        pthread_spin_init(&mutex_,0);
    }
    ~Spinlock(){
        pthread_spin_destroy(&mutex_);
    }

    void lock(){
        pthread_spin_lock(&mutex_);
    }
    void unlock(){
        pthread_spin_unlock(&mutex_);
    }
private:
    pthread_spinlock_t mutex_;
};
class CASLock{
public:
    typedef ScopedLockImpl<CASLock> Lock;
    CASLock(){
        mutex_.clear();
    }
    ~CASLock(){

    }

    void lock(){
        while(std::atomic_flag_test_and_set_explicit(&mutex_,std::memory_order_acquire));
    }
    void unlock(){
        std::atomic_flag_clear_explicit(&mutex_,std::memory_order_release);
    }
private:
    std::atomic_flag mutex_;
};

}

#endif