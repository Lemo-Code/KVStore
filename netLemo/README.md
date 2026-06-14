# netLemo — 极致性能协程网络库

在 `ucontext_t` 约束下重写的网络栈，基于稳定 `FdManager + FdContext + Reactor` 架构，叠加性能优化。

## 相对 lemo / module/net 的改进

| 优化 | 说明 |
|------|------|
| **poll → runnext 唤醒** | `triggerEventRunnext` 替代 `schedule`，IO 就绪走 runnext 槽，减少队列与 tickle |
| **Hook 无超时快路径** | echo 场景零 `shared_ptr<TimerInfo>` 堆分配 |
| **Fiber bootstrap ctx** | 池化 `reset` 跳过 `getcontext`，仅 `makecontext` |
| **FiberPool** | 协程对象 + 栈复用（TLS 缓存 256） |
| **StackPool** | 128KB 栈三层供给（TLS → global → malloc） |
| **idle spin 256** | 降低 runnext 路径 semaphore 唤醒延迟 |
| **close 去重** | 取消重复 `cancelAll` |

## 模块结构

```
utils → thread → memory(StackPool)
                    ↓
                 fiber(Fiber/Scheduler/Timer/FiberPool)
                    ↓
                 io(FdManager + Reactor + Hook + IOManager)
                    ↓
              buffer(RingBuffer) + socket
```

## 构建

```bash
cmake -B build-netlemo -DNETLEMO_BUILD=ON
cmake --build build-netlemo --target bench_echo_server_netlemo -j
```

产物：`bin/netLemo/bench_echo_server`

## 压测参考（aarch64, 4 线程, 64 连接, 128B）

| 模式 | QPS |
|------|-----|
| messages 1000 | **~280k–290k** |
| duration 10s（热身后） | **~180k–216k** |
| quick | **~146k** |

```bash
bin/netLemo/bench_echo_server --mode local --threads 4 --connections 64 --payload 128 --duration 10s
bin/netLemo/bench_echo_server --mode local --threads 4 --connections 64 --payload 128 --messages 1000
```

## 已知限制与后续

- **ucontext** 仍是主要瓶颈；messages 模式 ~290k 已接近 ucontext 上限，要追 Go ~500k 需 **fcontext**（Phase 2）
- **duration 首轮偏冷**（~56k），与 epoll/连接建立有关，热身后 ~216k
- 未合并 FdSlot（曾导致跨线程 resume 挂死），保持 FdManager 分离更稳
- Phase 3: RingBuffer 传输层、TcpServer、可选 io_uring
