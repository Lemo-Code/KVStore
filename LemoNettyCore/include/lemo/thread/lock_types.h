#pragma once

#include "lemo/thread/mutex.h"

namespace lemo {
namespace thread {

/**
 * 锁策略别名：各模块通过 MutexType 引用，便于集中调整。
 *
 * HotMutex    — 热路径、极短临界区、低竞争（本地 runq、reactor fd 槽）
 * ColdMutex   — 跨线程竞争或批量搬运（全局 runq、定时器桶）
 * RegistryMutex — 读多写少注册表（FdManager、Reactor fd 表）
 */
using HotMutex = Spinlock;
using ColdMutex = Mutex;
using RegistryMutex = RWMutex;

}  // namespace thread
}  // namespace lemo
