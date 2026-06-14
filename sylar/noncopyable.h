#ifndef __SYLAR_NONCOPYABLE_H__
#define __SYLAR_NONCOPYABLE_H__

namespace sylar {


    class Noncopyable
    {
    public:
        Noncopyable(){}
        ~Noncopyable(){}
        Noncopyable(const Noncopyable&) = delete;
        Noncopyable(const Noncopyable&&) = delete;

        Noncopyable& operator=(const Noncopyable&) = delete;
        Noncopyable& operator=(const Noncopyable&&) = delete;
    };

}



#endif