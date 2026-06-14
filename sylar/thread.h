#ifndef __SYLAR_THREAD_H__
#define __SYLAR_THREAD_H__
#include "sylar/noncopyable.h"

#include<pthread.h>
#include<semaphore.h>
#include<functional>
#include<string>
#include<memory>

namespace sylar
{

//信号量
class Semaphore: public Noncopyable
{
public:
    Semaphore(uint32_t count = 0);
    ~Semaphore();
    
    void wait();
    void notify();
private:
    sem_t semaphore_;
};

//线程
class Thread: public Noncopyable
{
public:
    typedef std::shared_ptr<Thread> ptr;
    Thread(std::function<void()> cb,const std::string& name);
    ~Thread();

    pid_t getId()const {return id_;}
    const std::string& getName()const {return name_;}

    void join();
    
    static Thread* GetThis();
    static const std::string& GetName();
    static void SetName(const std::string& name);
private:
    static void* run(void* arg);
private:
    pid_t id_ = -1;
    pthread_t thread_ = 0;
    std::function<void()> cb_;
    std::string name_;
    Semaphore semaphore_;   
};



}

#endif