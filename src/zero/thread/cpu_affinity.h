#pragma once

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#endif

#include <string>
#include <thread>

namespace zero {

// 设置当前线程的 CPU 亲和性 (绑定到指定核心)
inline bool SetCPUAffinity(int cpu_id) {
#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
#else
    (void)cpu_id;
    return false;
#endif
}

// 获取当前可用的 CPU 核心数
inline int GetCPUCount() {
    return static_cast<int>(std::thread::hardware_concurrency());
}

// 获取当前线程运行的 CPU 核心 ID
inline int GetCurrentCPU() {
#ifdef __linux__
    return sched_getcpu();
#else
    return -1;
#endif
}

} // namespace zero
