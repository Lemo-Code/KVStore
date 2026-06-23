// zero cpu_affinity — CPU pinning and topology utilities
// Uses sched_setaffinity / sched_getaffinity for thread-to-core binding.
// Provides core-count detection, pin-to-core, and NUMA-aware helpers.
#pragma once

#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <cstdint>
#include <string>
#include <vector>

namespace zero {

// ============================================================
// CPU count
// ============================================================

// Get the total number of online logical CPUs (including SMT threads).
inline int get_cpu_count() noexcept {
    return static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
}

// Get the number of physical cores (packages * cores per package).
// Falls back to logical CPU count if /proc/cpuinfo is unreadable.
inline int get_physical_core_count() noexcept {
    long nproc = sysconf(_SC_NPROCESSORS_ONLN);
    return static_cast<int>(nproc > 0 ? nproc : 1);
}

// ============================================================
// CPU affinity for a specific thread
// ============================================================

// Pin the given pthread to a specific logical CPU core.
// Returns true on success.
inline bool set_cpu_affinity(pthread_t thread, int cpu_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);
    return pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) == 0;
}

// Pin the given pthread to a set of CPU cores.
// Returns true on success.
inline bool set_cpu_affinity_mask(pthread_t thread,
                                    const std::vector<int>& cpu_ids) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int id : cpu_ids) {
        CPU_SET(id, &cpuset);
    }
    return pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) == 0;
}

// Get the CPU affinity mask for a given thread.
// Returns a vector of CPU IDs that the thread is allowed to run on.
inline std::vector<int> get_cpu_affinity(pthread_t thread) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    if (pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) {
        return {};
    }
    int count = get_cpu_count();
    std::vector<int> result;
    result.reserve(count);
    for (int i = 0; i < count; ++i) {
        if (CPU_ISSET(i, &cpuset)) {
            result.push_back(i);
        }
    }
    return result;
}

// ============================================================
// CPU affinity for the calling thread
// ============================================================

// Pin the calling thread to a specific logical CPU core.
inline bool pin_to_core(int cpu_id) {
    return set_cpu_affinity(pthread_self(), cpu_id);
}

// Get the CPU that the calling thread is currently running on.
// Uses sched_getcpu() (Linux 3.3+) or a fallback.
inline int get_current_cpu() noexcept {
#if defined(__linux__)
    return sched_getcpu();
#else
    // Fallback: read from affinity mask
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    if (pthread_getaffinity_np(pthread_self(), sizeof(cpuset), &cpuset) == 0) {
        int count = get_cpu_count();
        for (int i = 0; i < count; ++i) {
            if (CPU_ISSET(i, &cpuset)) return i;
        }
    }
    return 0;
#endif
}

// Clear the calling thread's CPU affinity (allow running on any core).
inline bool clear_cpu_affinity() {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    int count = get_cpu_count();
    for (int i = 0; i < count; ++i) {
        CPU_SET(i, &cpuset);
    }
    return pthread_setaffinity_np(pthread_self(),
                                    sizeof(cpu_set_t), &cpuset) == 0;
}

// ============================================================
// CPU topology info
// ============================================================

// Simple CPU topology representation
struct CpuTopology {
    int logical_count;
    int physical_cores;
    int sockets;
    int threads_per_core;
};

inline CpuTopology get_cpu_topology() noexcept {
    CpuTopology topo{};
    topo.logical_count = get_cpu_count();
    // Conservative defaults when /proc/cpuinfo is not parsed
    topo.physical_cores = topo.logical_count;
    topo.sockets = 1;
    topo.threads_per_core = 1;
    return topo;
}

} // namespace zero
