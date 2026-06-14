# Fiber 协程模块

`lemo::fiber` 提供用户态协程与 Go 风格调度器，是 IO/Reactor 的上层基础。

## 组件

| 类 | 职责 |
|----|------|
| `Fiber` | ucontext 非对称协程，`YieldToHold/Ready`、`SleepMs` |
| `Scheduler` | GMP 调度：本地/全局队列 + work stealing |
| `Timer` / `TimerManager` | 四层时间轮，供定时任务与 `Fiber::SleepMs` |
| `SchedulerSwitcher` | RAII 切换当前调度器 |

## 依赖

```
fiber → thread → utils
log   → fiber（GetFiberId / %F）
```

## 入口

```cpp
#include "lemo/fiber/module.h"

lemo::fiber::Scheduler sch(4, true, "main");
sch.start();
sch.schedule([]() { /* ... */ });
sch.stop();
```

## 线程模型

- 每个 pthread 有**主协程**（无独立栈）+ 多个**子协程**（独立栈，默认 128KB）
- `use_caller=true` 时，构造 Scheduler 的线程也参与调度（root fiber 跑 `run()` 循环）
- 任务优先级：`runnext` → local runq → global runq → steal

## 与日志

Pattern 支持 `%F`（fiber id），由 `lemo::fiber::GetCurrentFiberId()` 提供。

## 测试与基准

```bash
make run_tests_lemo_fiber          # 协程单元/集成（含 IOManager 上 SleepMs）
make run_tests_lemo_io             # Reactor / IOManager / FdManager
make run_tests_lemo                # 全量
bin/lemo/fiber/bench_scheduler --quick
```

与 IO 联动：`lemo::io::IOManager` 继承 Scheduler，`idle()` 内 epoll；`Fiber::SleepMs` 在 IOManager 上通过定时器 + epoll 驱动。

## 后续

- **hook**：阻塞 syscall 自动 yield（见 `lemo/io`）
- IO：`Reactor` 已独立，后续 `Runtime` 门面组合 Scheduler + Reactor
- 同步原语：`FiberMutex` / `FiberSemaphore`
- 栈切换：ucontext → fcontext
