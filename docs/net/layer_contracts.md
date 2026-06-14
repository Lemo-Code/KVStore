# 各层接口契约

本文定义各层 **公开 API** 的最小契约。实现代码须满足这些签名与语义；骨架头文件见 `module/net/design/`。

---

## L0 Base

### `net::Thread`

- `SetName` / `GetName` / `GetId`：pthread 封装，TLS 缓存线程 ID。
- 不感知协程。

### `net::Mutex` / `net::RWMutex` / `net::Semaphore`

- 标准 pthread 同步原语，供 **物理线程** 使用（调度器队列、Reactor fd 表）。
- 协程级同步见 L1 `FiberMutex`。

### `net::ConfigCenter`

- `lookup<T>(key, default)` 注册与读取配置项。
- 与日志配置解耦，不 include log 头文件。

---

## L1 Runtime

### `net::Context`

```cpp
class Context {
 public:
  using Entry = void(*)();
  Context(Entry entry, void* stack, size_t stack_size);
  void swapIn(Context& from);   // 切换到本上下文
  void swapOut(Context& to);    // 切换到目标上下文
};
```

- **唯一**负责栈分配与上下文切换的实现点。
- Phase 1：`ucontext`；Phase 2：`fcontext` 或平台汇编。

### `net::Fiber`

| 方法 | 语义 |
|------|------|
| `Fiber(cb, stack_size, use_caller)` | 创建子协程；`use_caller` 为调度器主协程模式 |
| `YieldToHold()` | 让出 CPU，保持 `HOLD`（可再次被调度） |
| `YieldToReady()` | 让出并置 `READY` |
| `SleepMs(ms)` | 向当前 Runtime 注册定时器后 yield |
| `GetThis()` / `GetFiberId()` | TLS 当前协程 |

### `net::Scheduler`

| 方法 | 语义 |
|------|------|
| `start()` / `stop()` | 启动/停止工作线程 |
| `schedule(fiber\|cb, pin_thread=-1)` | 入队；满则溢出到全局队列；必要时 `tickle` |
| `getTimer()` | 返回关联的 `TimerWheel&`（组合，非继承） |
| `GetThis()` / `GetProcessorId()` | TLS |

**不**提供 `epoll` 相关 API。

### `net::TimerWheel`

| 方法 | 语义 |
|------|------|
| `addTimer(delay_ms, cb) → TimerId` | 一次性定时器 |
| `addTimerRecurring(interval_ms, cb)` | 循环定时器 |
| `cancel(TimerId)` | 取消 |
| `nextTimeoutMs() → uint64_t` | 供 Reactor poll 超时；`UINT64_MAX` 表示无定时器 |

### `net::Runtime`（门面）

```cpp
class Runtime {
 public:
  Runtime(size_t threads, bool use_caller, std::string name);
  void start();
  void stop();
  Scheduler& scheduler();
  Reactor& reactor();
  static Runtime* GetThis();
};
```

- 工作线程主循环：`fetchTask → run fiber` 或 `reactor_.poll(nextTimeout) → 唤醒 fiber`。
- **替代旧 `IOManager`** 作为应用持有的顶层对象。

### `net::sync::FiberMutex` / `FiberSemaphore` / `FiberLocal<T>`

- 阻塞时 yield，唤醒时 `schedule(READY)`。
- `FiberLocal<T>`：协程退出时自动析构存储。

---

## L2 IO

### `net::Reactor`

| 方法 | 语义 |
|------|------|
| `addEvent(fd, READ\|WRITE, cb?)` | 注册事件；若提供 `cb` 则就绪时回调，否则唤醒挂起的 Fiber |
| `delEvent` / `cancelEvent` / `cancelAll` | 删除/取消 |
| `poll(timeout_ms) → int` | `epoll_wait` + 分发；返回就绪事件数 |
| `tickle()` | 写 pipe 唤醒阻塞在 poll 的线程 |

- **不继承** Scheduler；由 Runtime 在 `idle` 路径调用。

### `net::FdContext`

- 每 fd 一份：`is_socket`, `is_nonblock`, `recv_timeout`, `send_timeout`, `user_ctx`。
- 由 `FdManager` 单例按 fd 索引（fd 关闭时回收）。

### `net::hook`

| API | 语义 |
|-----|------|
| `hook_init()` | 进程启动时安装 hook（`dlsym`） |
| `set_hook_enable(bool)` | 线程级开关 |
| `is_hook_enable()` | 查询 |

---

## L3 Transport

### `net::Address`

- IPv4/IPv6/Unix 地址解析与 `sockaddr` 转换。

### `net::Socket`

- 工厂方法 `CreateTCP/UDP`；`bind/connect/listen/accept/send/recv`。
- **默认路径**：依赖 hook，不自行 `epoll`。
- 超时通过 `FdContext` + `SO_RCVTIMEO`/`SO_SNDTIMEO` 与 hook 协同。

### `net::Stream` / `SocketStream`

- 抽象 `read/write/readExactly/writeExactly`；供协议层使用。

### `net::RingBuffer`

- 2 的幂容量 kfifo；`readFd`/`writeFd` 对接 `readv`/`writev` 零拷贝。

---

## L4 Server

### `net::Acceptor`

- 构造：`Address::ptr listen_addr, Runtime* rt`。
- `start(cb)`：`listen` + 循环 `accept`，每连接 `rt->scheduler().schedule([=]{ cb(sock); })`。

### `net::TcpServer`

- 多 worker `Runtime` 组；`bind` / `start` / `stop`。
- `setAcceptHandler(ConnectionHandler)` 设置业务回调。

### `net::WorkerGroup`

- 按 CPU 或配置创建多个 `Runtime`，RR 或 hash 分发新连接。

---

## 日志（正交）

- 宏 `NET_LOG_*` 保持不变。
- `net_log_obj` **仅**链接 `net_base_obj`，不链接 `net_io_obj`。
- `Formatter` 中 `%F`（fiber id）通过 `GetFiberId()` 弱依赖，避免 log → runtime 头文件环。

---

## 错误处理约定

| 场景 | 约定 |
|------|------|
| syscall 失败 | 设置 `errno`，返回 `-1`；与 POSIX 一致 |
| hook 中 EAGAIN | yield 后重试 |
| 协程未捕获异常 | 状态 `EXCEPT`，记录日志，终止协程 |
| `stop()` | 先停 Acceptor，取消全部 fd 事件，drain 队列后 join 线程 |
