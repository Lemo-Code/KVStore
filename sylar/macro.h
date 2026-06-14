#ifndef __SYLAR_MACRO_H__
#define __SYLAR_MACRO_H__
#include "sylar/log.h"

#include <assert.h>
#include <execinfo.h>

#if defined __GNUC__ || defined __llvm__
#define SYLAR_LICKLY(x) __builtin_expect(!!(x), 1)
#define SYLAR_UNLICKLY(x) __builtin_expect(!!(x), 0) // 表示预期值为0的概率比较高
#else
#define SYLAR_LIKELY(x) (x)
#define SYLAR_UNLICKLY(x) (x)
#endif

//x -> false
#define SYLAR_ASSERT(x) \
    if(!(x)){   \
        SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ASSERTION: " #x   \
            <<  "\nbacktrace:\n"    \
            <<  sylar::BacktraceToString(100,2,"    "); \
        assert(x);  \
    }

#define SYLAR_ASSERT2(x,w) \
    if(!(x)){   \
        SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ASSERTION: " #x   \
            << "\n" << w   \
            <<  "\nbacktrace:\n"    \
            <<  sylar::BacktraceToString(100,2,"    "); \
        assert(x);  \
    }

#endif