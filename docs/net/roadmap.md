# 实施路线图

按 **依赖自底向上** 分六阶段重写；每阶段可独立合并，旧代码在同目录旁路保留直至验收通过。

---

## Phase 0：架构准备（当前）

| 交付物 | 状态 |
|--------|------|
| `docs/net/architecture.md` | ✅ |
| `docs/net/layer_contracts.md` | ✅ |
| `module/net/design/` 接口骨架 | ✅ |
| CMake 命名方案（`LSTL_BUILD_NET` / `net::net`） | 文档化，Phase 1 落地 |

**不写实现代码**，仅冻结接口与目录。

---

## Phase 1：Runtime 核心

**目录**：`module/net/runtime/`（新建，与旧 `fiber/` 并行）

| 任务 | 说明 |
|------|------|
| `Context` | 抽象栈切换；先封装 `ucontext`，接口稳定后换 `fcontext` |
| `Fiber` | 迁入并精简状态机 |
| `RunQueue` | 从旧 `run_queue.h` 迁入 |
| `TimerWheel` | 从旧 `timing_wheel` 迁入 |
| `Scheduler` | **去掉** 继承 `TimerManager`，改为组合 |
| `Runtime` | 骨架：仅调度循环，暂不接 epoll |

**测试**：`test_fiber` `test_scheduler` `test_timer` `test_timer_wheel`  
**基准**：`bench_scheduler` 不低于旧版 90%

**删除/退役**：验收后移除旧 `fiber/scheduler.h` 中的 Timer 继承关系。

---

## Phase 2：IO 层

**目录**：`module/net/io/`（重写）

| 任务 | 说明 |
|------|------|
| `Reactor` | 从 `IOManager` 剥离 epoll 逻辑；不继承 Scheduler |
| `FdContext` / `FdManager` | 精简，统一超时语义 |
| `hook` | 统一 connect 路径；删除 `connect.cc` 旁路 |
| `Runtime::idle` | 集成 `reactor_.poll(timer_.nextTimeoutMs())` |

**测试**：`test_reactor`（新） `test_hook` `test_iomanager`（迁移）  
**验收**：hook 下 `read`/`sleep`/`connect` 正确 yield 与唤醒。

---

## Phase 3：Transport

**目录**：`module/net/transport/`（自 `socket/` + `buffer/` 迁入）

| 任务 | 说明 |
|------|------|
| `Address` `Socket` `Stream` | API 保持，内部仅走 hook |
| `RingBuffer` `ByteArray` | 自旧 `buffer/` 迁入 |

**测试**：`test_address` `test_ring_buffer` `test_byte_array` + 新增 `test_socket_loopback`

---

## Phase 4：Server

**目录**：`module/net/server/`（新建）

| 任务 | 说明 |
|------|------|
| `Acceptor` | listen + accept + schedule |
| `TcpServer` | 多 Runtime worker |
| `WorkerGroup` | 连接分发策略 |

**测试**：重写 `bench_echo_server` 使用 `TcpServer`  
**验收**：QPS ≥ 旧 `bench_echo_server`。

---

## Phase 5：Sync 原语

**目录**：`module/net/runtime/sync/`

| 任务 | 说明 |
|------|------|
| `FiberMutex` | 竞争失败 yield |
| `FiberSemaphore` | 参考 sylar 语义，修正命名 |
| `FiberLocal<T>` | 协程 TLS |

**测试**：`test_fiber_mutex` `test_fiber_sem` `test_fiber_local`（新建）

---

## Phase 6：整合与清理

| 任务 | 说明 |
|------|------|
| CMake | `LSTL_BUILD_NET` → 目标 `net`，别名 `net::net`；`log` 可选 `net_full` |
| 删除旧目录 | `fiber/` 合并入 `runtime/`；废弃 `IOManager` 类名 |
| 文档 | 更新 `module/README.md`、根 `README.md`；标注 `sylar/` 为 frozen |
| 脚本 | 修复 `run_scheduler_perf.sh` 等路径；增加 C++/Go echo 对比脚本 |
| `design/` | 接口稳定后删除或归档 |

---

## 风险与对策

| 风险 | 对策 |
|------|------|
| `ucontext` 不可用 | `Context` 抽象 + Phase 1 末引入 `fcontext` |
| 重构期间双份代码 | 旧目录保留到 Phase 验收；CMake 用选项切换链接目标 |
| 日志耦合 | Phase 6 前不改 log 实现，仅改 CMake 依赖边 |

---

## 当前建议的下一步

从 **Phase 1** 开始：在 `module/net/runtime/` 实现 `Context` + `Fiber` + `Scheduler`（组合定时器），并跑通 `test_scheduler`。
