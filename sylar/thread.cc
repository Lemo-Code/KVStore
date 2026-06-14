#include "sylar/thread.h"
#include "sylar/log.h"

namespace sylar
{

static thread_local Thread* t_thread = nullptr;
static thread_local std::string t_thread_name = "UNKNOWN";

static sylar::Logger::ptr g_logger = SYLAR_LOG_NAME("system");

Thread* Thread::GetThis(){
    return t_thread;
}

const std::string& Thread::GetName(){
    return t_thread_name;
}

void Thread::SetName(const std::string& name){
    if(t_thread){
        t_thread->SetName(name);
    }
    t_thread_name = name;
}

Thread::Thread(std::function<void()> cb,const std::string& name)
        : cb_(cb)
        , name_(name){
    
    if(name_.empty()){
        name_ = "UNKNOWN";
    }
    int rt = pthread_create(&thread_,nullptr,&Thread::run,this);
    if(rt){
        SYLAR_LOG_ERROR(g_logger) << "pthread_create thread fail, rt=" << rt
                << "name=" << name;
        throw std::logic_error("pthread_create error");
    }
    //这个信号量是属于每一个线程的，保证new线程和主线程的同步 而不是保证new线程之间的互斥关系
    semaphore_.wait();
}

Thread::~Thread(){
    if(thread_){
        pthread_detach(thread_);
    }
}

void Thread::join(){
    if(thread_){
        int rt = pthread_join(thread_,nullptr);
        if(rt) {
            SYLAR_LOG_ERROR(g_logger) << "pthread_join thread fail, rt=" 
                << rt << " name=" << name_;
            throw std::logic_error("pthread_join error"); 
        }
        thread_ = 0;
    }
}

//static  arg = this
void* Thread::run(void* arg){
    Thread* thread = (Thread*)arg;
    t_thread = thread;
    t_thread->id_ = sylar::GetThreadId();
    t_thread_name = thread->getName();
    pthread_setname_np(pthread_self(),thread->name_.substr(0,15).c_str());

    std::function<void()> cb;
    //防止cb_有智能指针出现计数不能减为0的情况
    cb.swap(thread->cb_);

    thread->semaphore_.notify();
    
    cb();
    return 0;
}


Semaphore::Semaphore(uint32_t count){
    if(sem_init(&semaphore_,0,count)){
        throw std::logic_error("sem_init error");
    }
}

Semaphore::~Semaphore(){
    sem_destroy(&semaphore_);
}

void Semaphore::wait(){
    if(sem_wait(&semaphore_)){
        throw std::logic_error("sem_wait error");
    }
}

void Semaphore::notify(){
    if(sem_post(&semaphore_)){
        throw std::logic_error("sem_post error");
    }
}


}