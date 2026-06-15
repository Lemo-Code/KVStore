# Zero - 高性能 Linux 网络库架构设计

> 借鉴 Go netpoller、Seastar、muduo 的精华设计，从零构建。

---

## 一、核心理念

### 1.1 设计哲学

| 原则 | 说明 |
|------|------|
| **Per-thread everything** | 每线程独立的 epoll + timer + 任务队列，消除全局锁竞争 |
| **M:N fiber scheduling** | M 个 fiber 运行在 N 个线程上，类似 Go 的 G-M-P 模型 |
| **Lock-free fast path** | 热路径无锁 (MPMC work-stealing queue)；冷路径用 RWLock/RCU |
| **No virtual on hot path** | 热路径用模板/编译期多态替代虚函数 |
| **Memory pooling** | fiber栈池、buffer块池、对象池 → 消除频繁 malloc |
| **Zero-copy** | scatter/gather IO (iovec)、splice/sendfile |
| **Cache-line friendly** | 热数据 cache-line 对齐，避免 false sharing |

### 1.2 与 Go netpoller 的对比

```
Go 模型:  G (goroutine)  ──  M (OS thread)  ──  P (processor)
Zero 模型: F (fiber)     ──  T (OS thread)  ──  R (reactor, per-thread epoll)

当一个 fiber 调用 read() 时:
  1. Hook 层拦截 → 设置 fd 为 non-blocking
  2. 调用真实 read() → EAGAIN
  3. 向当前线程的 reactor 注册 READ 事件
  4. fiber yield → scheduler 调度下一个 fiber
  5. reactor epoll_wait 返回就绪 → fiber 标记为 runnable
  6. scheduler 恢复 fiber → 重试 read()
```

---

## 二、整体分层架构

```
┌──────────────────────────────────────────────────────────────────┐
│                     Application Layer                            │
│        (HTTP / RPC / 自定义协议 可基于 Stream 层构建)              │
├──────────────────────────────────────────────────────────────────┤
│  ┌─────────────────────┐  ┌──────────────────────────────────┐  │
│  │    Stream Layer      │  │        Buffer Layer              │  │
│  │  - SocketStream      │  │  - ByteBuffer (链式块)           │  │
│  │  - BufferedStream    │  │  - iovec 零拷贝读写              │  │
│  │  - SSLStream         │  │  - BlockPool (内存池)            │  │
│  └─────────────────────┘  └──────────────────────────────────┘  │
├──────────────────────────────────────────────────────────────────┤
│  ┌─────────────────────┐  ┌──────────────────────────────────┐  │
│  │    Socket Layer      │  │        Address                   │  │
│  │  TCP / UDP / Unix    │  │  IPv4 / IPv6 / Unix              │  │
│  │  non-blocking 操作    │  │  DNS (异步协程)                  │  │
│  └─────────────────────┘  └──────────────────────────────────┘  │
├──────────────────────────────────────────────────────────────────┤
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                    Hook Layer                              │   │
│  │  dlsym(RTLD_NEXT) 劫持:  read/write/send/recv/connect...  │   │
│  │  Fiber 感知:  阻塞 → 注册 epoll → yield → 恢复 → 重试     │   │
│  └──────────────────────────────────────────────────────────┘   │
├──────────────────────────────────────────────────────────────────┤
│  ┌─────────────── Scheduler (M:N纤维调度) ───────────────────┐  │
│  │                                                             │  │
│  │  Thread-0          Thread-1          Thread-N               │  │
│  │  ┌───────────┐    ┌───────────┐    ┌───────────┐           │  │
│  │  │ Local LIFO│    │ Local LIFO│    │ Local LIFO│           │  │
│  │  │ fiber Q   │◄──►│ fiber Q   │◄──►│ fiber Q   │  ← work  │  │
│  │  │           │    │           │    │           │    steal   │  │
│  │  ├───────────┤    ├───────────┤    ├───────────┤           │  │
│  │  │ Reactor   │    │ Reactor   │    │ Reactor   │           │  │
│  │  │ ┌───────┐ │    │ ┌───────┐ │    │ ┌───────┐ │           │  │
│  │  │ │ epoll │ │    │ │ epoll │ │    │ │ epoll │ │(per-thr) │  │
│  │  │ ├───────┤ │    │ ├───────┤ │    │ ├───────┤ │           │  │
│  │  │ │Timer  │ │    │ │Timer  │ │    │ │Timer  │ │(per-thr) │  │
│  │  │ │Wheel  │ │    │ │Wheel  │ │    │ │Wheel  │ │           │  │
│  │  │ └───────┘ │    │ └───────┘ │    │ └───────┘ │           │  │
│  │  └───────────┘    └───────────┘    └───────────┘           │  │
│  └─────────────────────────────────────────────────────────────┘  │
├──────────────────────────────────────────────────────────────────┤
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                   Fiber Layer                              │   │
│  │  - Stackful coroutine  (汇编级 ctx switch, ~10ns)         │   │
│  │  - Stack pool (栈内存复用池)                               │   │
│  │  - Fiber-local storage                                    │   │
│  │  - 同步原语:  Channel / FiberMutex / WaitGroup             │   │
│  └──────────────────────────────────────────────────────────┘   │
├──────────────────────────────────────────────────────────────────┤
│  ┌──────────────────────────────────────────────────────────┐   │
│  │                   Thread Layer                             │   │
│  │  - pthread 封装 + CPU affinity                            │   │
│  │  - SpinLock / RWLock / Semaphore                          │   │
│  └──────────────────────────────────────────────────────────┘   │
├──────────────────────────────────────────────────────────────────┤
│  ┌──────────────┐  ┌──────────────────┐  ┌─────────────────┐    │
│  │  Log (异步)   │  │  Config (RCU)    │  │  Util / 公共     │    │
│  │  fmt + ring   │  │  YAML + 热加载   │  │  基础设施        │    │
│  └──────────────┘  └──────────────────┘  └─────────────────┘    │
├──────────────────────────────────────────────────────────────────┤
│                    Linux Kernel (epoll / io_uring)                │
└──────────────────────────────────────────────────────────────────┘
```

---

## 三、目录结构

```
zero/                              # 项目名 "zero"（零开销网络库）
├── CMakeLists.txt
├── zero.h                         # 顶层聚合头文件
│
├── base/                          # 基础工具层
│   ├── noncopyable.h
│   ├── macro.h                    # likely/unlikely/assert
│   ├── endian.h                   # 字节序转换
│   ├── singleton.h                # 单例模板
│   └── lexicalcast.h              # 类型转换
│
├── log/                           # 日志模块
│   ├── log_level.h                # 日志级别
│   ├── log_event.h                # 日志事件
│   ├── log_formatter.h            # 格式器 (fmt 库)
│   ├── log_appender.h             # 输出器
│   ├── logger.h                   # 日志器
│   ├── async_logger.h             # 异步日志 (lock-free ring buffer)
│   └── log.h                      # 便捷宏
│
├── config/                        # 配置模块
│   ├── config_var.h               # 配置项
│   ├── config_manager.h           # 配置管理器 (RCU 热加载)
│   └── yaml_parser.h
│
├── thread/                        # 线程模块
│   ├── thread.h                   # Thread 封装
│   ├── mutex.h                    # SpinLock / Mutex / RWLock
│   ├── semaphore.h
│   └── cpu_affinity.h
│
├── fiber/                         # 协程模块
│   ├── context.h                  # 汇编级上下文切换 (x86_64/ARM64)
│   ├── fiber.h                    # Fiber 协程
│   ├── fiber_pool.h               # Fiber 对象池
│   ├── stack_pool.h               # 栈内存池
│   ├── fiber_local.h              # Fiber 局部存储
│   ├── channel.h                  # Go 风格 Channel (MPSC)
│   ├── fiber_mutex.h              # Fiber 感知的互斥锁
│   └── waitgroup.h
│
├── scheduler/                     # 调度器 + Reactor
│   ├── scheduler.h                # M:N 调度器
│   ├── work_stealing_queue.h      # Lock-free work-stealing 队列
│   ├── reactor.h                  # Per-thread epoll 事件循环
│   ├── fd_context.h               # Per-fd 事件回调
│   ├── timer_wheel.h              # 分层时间轮
│   └── hook.h                     # Syscall hook
│
├── net/                           # 网络层
│   ├── address.h                  # IPv4 / IPv6 / Unix 地址
│   ├── socket.h                   # TCP / UDP / Unix Socket
│   ├── socket_ops.h               # 底层 socket 操作
│   ├── buffer.h                   # 链式 Buffer (ByteArray)
│   ├── stream.h                   # 抽象 Stream 接口
│   ├── socket_stream.h            # Socket Stream
│   ├── buffered_stream.h          # 缓冲 I/O Stream
│   └── tcp_server.h               # TCP Server 辅助
│
└── util/                          # 工具集
    ├── util.h
    ├── hash_util.h
    └── json_util.h
```

---

## 四、各模块详细设计

### 4.1 Fiber (协程) — 重构优先级: ⭐⭐⭐⭐⭐

**sylar 问题:** ucontext 系统调用慢、malloc/free 每次分配栈、固定栈大小、reset 重设 context 开销大。

**新设计:**

```
核心改进:
  1. 汇编级上下文切换 (参考 boost.context / libco)
     - x86_64: swapcontext 替代 → 手动保存/恢复 callee-saved 寄存器
     - 关键: 仅保存 rbx/rbp/r12-r15/rsp/rip，跳过浮点寄存器 (延迟保存)
     - 一次切换 ~10-15ns vs ucontext ~200-300ns

  2. 栈内存池 (StackPool)
     - 默认 128KB 栈, 池化复用
     - 支持 guard page (mmap + mprotect) 检测栈溢出
     - 可选: growable stack (split stack, 类似 Go 1.x)

  3. Fiber 对象池
     - Fiber 对象本身也池化复用 (避免 shared_ptr 频繁分配)
     - reset() 方法复用栈空间，重置执行函数

  4. Fiber 状态机精简: IDLE → RUNNING → (HOLD | READY | TERM)

  5. Fiber-local storage
     - 每个 fiber 独立的 TLS (不依赖 pthread_key)
     - 哈希表存储，fiber 切换时自动切换
```

```cpp
// fiber/context.h - 上下文切换接口
class FiberContext {
public:
    // 保存当前上下文到 from, 恢复 to 的上下文
    static void Swap(FiberContext* from, FiberContext* to) noexcept;
    
    // 初始化上下文: 设置栈指针和入口函数
    void init(void* stack_base, size_t stack_size, void (*entry)(void*), void* arg);
    
private:
    void* rsp_;     // 栈指针
    // ... callee-saved registers
};
```

### 4.2 Scheduler (调度器) — 重构优先级: ⭐⭐⭐⭐⭐

**sylar 问题:** 全局 task queue + Mutex 争用、O(n) 链表遍历、无 work-stealing。

**新设计: Go 风格 M:N 调度器**

```
核心改进:
  1. Per-thread LIFO queue (本地队列, lock-free)
     - 当前线程 spawn 的 fiber 放入本地队列
     - 空闲线程从其他线程的本地队列 steal (FIFO 端取)
     - 使用 Chase-Lev work-stealing deque

  2. Global MPSC queue (全局队列, lock-free)
     - 非本地线程 spawn 的 fiber 放入全局队列
     - 线程定期从全局队列 poll

  3. 调度循环 (每线程):
     while(!stopping) {
         // 1. 优先从本地队列取 (LIFO, cache hot)
         fiber = local_queue.pop();
         
         // 2. 本地空 → 从全局队列取
         if(!fiber) fiber = global_queue.pop();
         
         // 3. 全局空 → work-stealing 从其他线程取
         if(!fiber) fiber = steal_from_random();
         
         // 4. 全空 → epoll_wait (在 Reactor 中)
         if(!fiber) reactor.poll(timeout);
         
         // 5. 执行 fiber
         if(fiber) fiber->resume();
     }

  4. 阻塞操作处理:
     - fiber 调用阻塞 syscall → hook 层拦截 → 注册到 reactor → fiber yield
     - 调度器自然切换到下一个 runnable fiber，无需额外线程
     - 真正阻塞的操作 (如 gethostbyname): dispatch 到 dedicated 线程池
```

```cpp
// scheduler/work_stealing_queue.h
// Chase-Lev 双端队列: 拥有者从 LIFO 端 push/pop, 窃取者从 FIFO 端 steal
class WorkStealingQueue {
    static constexpr size_t CAPACITY = 256;
    
    // 仅拥有者调用 (LIFO)
    bool push(Fiber* fiber);
    Fiber* pop();       // LIFO pop
    
    // 其他线程调用 (FIFO steal)
    Fiber* steal();
    
private:
    alignas(64) std::atomic<size_t> top_;     // LIFO 端
    alignas(64) std::atomic<size_t> bottom_;  // FIFO 端
    alignas(64) std::array<Fiber*, CAPACITY> buffer_;
};
```

### 4.3 Reactor (epoll 事件循环) — 重构优先级: ⭐⭐⭐⭐⭐

**sylar 问题:** 全局单 epoll fd 多线程竞争；FdContext 全局 vector + RWMutex。

**新设计: Per-thread Reactor + Timer Wheel**

```
核心改进:
  1. 每线程独立 epoll 实例
     - Thread-0 拥有的 fd 注册在 Thread-0 的 epoll
     - 无跨线程 epoll 竞争
     - 使用 EPOLLONESHOT / EPOLLET 边缘触发

  2. FdContext 嵌入 Reactor
     - 每个 Reactor 维护自己的 fd → EventCtx 映射
     - 不用全局 vector，用 per-reactor 的 unordered_map 或数组
     - 注册/取消操作无全局锁

  3. Timer Wheel 嵌入 Reactor (见下节)

  4. 与 Scheduler 的集成:
     - Reactor::poll() 替代 scheduler idle()
     - epoll_wait 超时 = min(next_timer, 无任务时的小超时)
     - epoll 返回事件 → 恢复对应 fiber → 放入 local queue
```

```cpp
// scheduler/reactor.h
class Reactor {
public:
    Reactor();
    
    // 注册/取消 IO 事件 (仅本线程调用，无锁)
    int addEvent(int fd, Event event, Fiber* waiter);
    bool delEvent(int fd, Event event);
    bool cancelEvent(int fd, Event event);
    
    // 事件循环的一轮: epoll_wait + 触发回调
    // timeout_ms: 最长等待时间 (由 timer wheel 的下一个到期时间决定)
    // 返回: 被唤醒的 fiber 数量
    int poll(int timeout_ms);
    
    // 唤醒 epoll_wait (用于跨线程通知)
    void notify();
    
private:
    int epoll_fd_;
    int event_fd_;               // eventfd 替代 pipe (更快)
    
    struct EventCtx {
        Fiber* read_waiter = nullptr;
        Fiber* write_waiter = nullptr;
        int registered_events = 0;
    };
    
    std::vector<EventCtx> fd_ctxs_;  // 按 fd 索引
};
```

### 4.4 Timer Wheel (定时器) — 重构优先级: ⭐⭐⭐⭐

**sylar 问题:** `std::set` O(log n) + RWMutex → 大规模定时器性能极差。

**新设计: 分层时间轮 (Hierarchical Timer Wheel)**

```
设计:
  - 借鉴 Linux 内核 timer wheel
  - 5 层时间轮, 覆盖 1ms ~ 2^32 ms (~49天)
  - Layer 0: 256 slots × 1ms    → 覆盖 0~255ms
  - Layer 1: 64 slots  × 256ms  → 覆盖 256ms~16s  
  - Layer 2: 64 slots  × 16s    → 覆盖 16s~17min
  - Layer 3: 64 slots  × 17min  → 覆盖 17min~18h
  - Layer 4: 64 slots  × 18h    → 覆盖 18h~49d

  - 插入: O(1) 定位层级 + O(1) 放入 slot 链表
  - 到期: 每 ms tick 只检查 Layer 0 当前 slot，O(1)
  - 降级 (cascade): slot 粒度到期时，定时器从高层降级到更低层
  
  - Per-thread: 每线程独立的时间轮，无需全局锁
```

```cpp
// scheduler/timer_wheel.h
class TimerWheel {
public:
    using TimerCallback = std::function<void()>;
    
    // 添加定时器, 返回 TimerId 用于取消
    TimerId addTimer(uint64_t delay_ms, TimerCallback cb, bool recurring = false);
    
    // 取消定时器
    bool cancelTimer(TimerId id);
    
    // 推进时钟, 收集到期的回调
    // now_ms: 当前时间戳 (毫秒)
    // cbs: 输出参数, 到期的回调列表
    void tick(uint64_t now_ms, std::vector<TimerCallback>& cbs);
    
    // 获取下一个定时器到期时间 (用于 epoll_wait 超时计算)
    // 返回 ~0ull 表示无定时器
    uint64_t nextExpireMs() const;
    
private:
    struct TimerSlot {
        struct TimerNode {
            uint64_t expire_ms;
            uint64_t interval_ms;  // 0 = 一次性
            TimerCallback cb;
            TimerNode* next;
        };
        TimerNode* head = nullptr;
    };
    
    static constexpr int NUM_LEVELS = 5;
    // L0: 256 slots, L1-L4: 64 slots each
    // ... implementation
};
```

### 4.5 Hook (系统调用劫持) — 重构优先级: ⭐⭐⭐

**sylar 问题:** 复用度不错但 `goto RETRY` 模式丑、耦合 IOManager 太紧。

**新设计: 保持大方向, 解耦化重构**

```cpp
// scheduler/hook.h
// 核心改进:
// 1. 使用 RAII 替代 goto RETRY
// 2. 抽象 "阻塞等待" 为统一的 yield-and-retry 原语
// 3. 通过 Reactor* GetCurrentReactor() 获取当前线程 reactor

// 简化的 do_io 模板
template <typename F, typename... Args>
ssize_t do_io(int fd, F&& real_fn, int event, int timeout_opt, Args&&... args) {
    if (!is_hook_enabled()) {
        return real_fn(fd, std::forward<Args>(args)...);
    }
    
    FdCtx* ctx = get_fd_ctx(fd);
    if (!ctx || !ctx->is_socket || ctx->user_nonblock) {
        return real_fn(fd, std::forward<Args>(args)...);
    }
    
    Reactor* reactor = GetCurrentReactor();
    
    for (;;) {
        ssize_t n = real_fn(fd, std::forward<Args>(args)...);
        while (n == -1 && errno == EINTR)
            n = real_fn(fd, std::forward<Args>(args)...);
        
        if (n != -1 || errno != EAGAIN)
            return n;
        
        // 注册事件 + 设置超时定时器 + yield
        auto timer_id = reactor->addTimer(ctx->timeout[timeout_opt],
            [reactor, fd, event] { reactor->cancelEvent(fd, (Event)event); });
        
        reactor->addEvent(fd, (Event)event, GetCurrentFiber());
        GetCurrentFiber()->yield();
        reactor->cancelTimer(timer_id);
        
        if (fiber_timed_out())
            return -1;
    }
}
```

### 4.6 Log (日志) — 重构优先级: ⭐⭐⭐⭐

**sylar 问题:** stringstream 慢、Mutex 竞争、无编译期格式检查。

**新设计: 混合同步/异步, 基于 fmt 库**

```
核心改进:
  1. 使用 {fmt} 库 (C++20 std::format) 替代 stringstream
     - 编译期格式字符串检查
     - 10x+ 性能提升
     - 零拷贝格式化

  2. 异步 logger: lock-free SPSC ring buffer
     - app 线程写入 ring buffer (无锁)
     - 后台 writer 线程消费并落盘
     - 类似 spdlog 的 async logger

  3. 同步 logger: 用于关键路径/调试, 直接输出

  4. Appender 体系:
     - ConsoleAppender (stdout/stderr, 支持颜色)
     - FileAppender (单文件)
     - RollingFileAppender (按大小/时间轮转 + 压缩)
     - MemoryAppender (环形缓冲, 仅保留最近 N 条)
```

### 4.7 Config (配置) — 重构优先级: ⭐⭐⭐

**sylar 问题:** 每个 ConfigVar 单独 RWMutex。

**新设计: RCU (Read-Copy-Update)**

```
核心改进:
  1. RCU 读取: 无锁读, config 读取不阻塞任何线程
  2. 写入时: 拷贝 → 修改 → 原子替换指针 → 等 RCU grace period → 释放旧值
  3. YAML 文件热加载 + 变更回调 (异步通知)
  4. 分层配置: 命令行 > 环境变量 > 配置文件 > 默认值
```

### 4.8 Buffer (字节缓冲) — 重构优先级: ⭐⭐⭐

**设计: 链式块缓冲区 (类似 sylar ByteArray, 但优化)**

```
核心设计:
  - 双向读写指针 (read_position / write_position)
  - Block size: 4KB (默认, 可配置)
  - 支持 scatter/gather IO:
    * getReadBuffers() → vector<iovec>
    * getWriteBuffers() → vector<iovec>
  - Block 内存池: 频繁分配/释放的 4KB 块从池中取
  - 序列化支持 (固定长度 / Varint)
  - 大端/小端自适应
```

### 4.9 Stream (流抽象) — 重构优先级: ⭐⭐⭐

**设计: 精简虚函数接口**

```cpp
class Stream {
public:
    // 核心接口 (纯虚)
    virtual ssize_t read(void* buf, size_t len) = 0;
    virtual ssize_t write(const void* buf, size_t len) = 0;
    virtual void close() = 0;
    
    // 非虚: 循环读/写直到指定长度 (由纯虚 read/write 实现)
    ssize_t readFixed(void* buf, size_t len);
    ssize_t writeFixed(const void* buf, size_t len);
    
    // Buffer 接口 (非虚, 调用纯虚 read/write)
    ssize_t read(ByteBuffer& buf, size_t len);
    ssize_t write(ByteBuffer& buf, size_t len);
};

// 实现类
class SocketStream : public Stream { ... };    // 直接 socket IO
class BufferedStream : public Stream { ... };  // 用户态缓冲 + 批量 flush
class SSLStream : public Stream { ... };       // OpenSSL 封装
```

### 4.10 Address / Socket — 重构优先级: ⭐⭐

**保持 sylar 的良好设计, 主要优化:**
- 减少虚函数调用, 用 enum-based dispatch
- Socket IO 直接 inline, 不经过中间层
- 批量 accept (一次 accept 多个连接减少系统调用)

---

## 五、关键数据流

### 5.1 Fiber 读操作的完整路径

```
Application:  stream.read(buf, len)
    │
    ▼
SocketStream:  ::read(fd, buf, len)         // 被 hook 劫持
    │
    ▼
hook::read():  if(!hook_enabled) → real_read()
               if(!is_socket || user_nonblock) → real_read()
    │
    ▼
               n = real_read(fd, buf, len)  // 真实系统调用
               if(n >= 0) → return n        // 成功, 直接返回
               if(errno != EAGAIN) → return -1
    │
    ▼ (EAGAIN: 资源暂时不可用)
               注册事件: reactor->addEvent(fd, READ, current_fiber)
               设置超时: reactor->addTimer(timeout, cancel_read)
    │
    ▼
               当前 fiber yield
               调度器切换到下一个 runnable fiber
    │
    ▼ (epoll 返回此 fd 可读)
               reactor 取消超时定时器
               reactor 将 fiber 标记为 RUNNABLE
               调度器恢复 fiber
    │
    ▼
               fiber 回到 do_io 循环, goto retry
               n = real_read(fd, buf, len)  → 这次成功返回
```

### 5.2 跨线程任务调度

```
Thread-A fiber spawns a new fiber for Thread-B:
  
  1. Thread-A: 创建 FiberB, 设置 target_thread = B
  2. Thread-A: 将 FiberB push 到 Thread-B 的 local queue (CAS)
  3. Thread-A: 如果 Thread-B 正在 epoll_wait, 需要唤醒:
     → write(Thread-B.reactor.event_fd, &wake, sizeof(wake))
  4. Thread-B: epoll_wait 返回 event_fd 可读
     → 从 local queue pop FiberB
     → 执行 FiberB
```

---

## 六、性能对标预期

| 场景 | 目标 | 对比 |
|------|------|------|
| context switch | ~10ns | ucontext ~200ns |
| fiber spawn | ~50ns | sylar ~500ns |
| echo server (per core) | ~100K QPS | nginx ~100K QPS |
| 100K timers tick | ~1μs | sylar ~100μs |
| 单线程 IO 调度 | 无锁 | sylar 全局锁 |

---

## 七、实施计划

| 阶段 | 内容 | 预计产出 |
|------|------|---------|
| **Phase 1** | base + thread + fiber + context | Fiber 可独立运行、切换 |
| **Phase 2** | scheduler + work_stealing_queue | M:N 调度器跑通 |
| **Phase 3** | reactor + timer_wheel + hook | 完整的协程化 IO |
| **Phase 4** | buffer + address + socket + stream | 网络层可用 |
| **Phase 5** | log + config | 基础设施完备 |
| **Phase 6** | tcp_server + examples + benchmark | 可演示、可压测 |

---

## 八、终裁决策 (架构师裁定)

| # | 决策点 | 裁定 | 理由 |
|---|--------|------|------|
| 1 | **命名空间** | `zero` | 简洁，零开销寓意 |
| 2 | **C++ 标准** | **C++17** | 用户确认；广泛兼容，有 `if constexpr`/`string_view`/`variant`；C++20 coroutine 我们不用 |
| 3 | **io_uring** | epoll Phase 1；io_uring 作为可选编译后端 Phase 2 | Reactor 设计抽象接口，可插拔；先进 Linux 也兼容旧内核 |
| 4 | **SSL** | **可选扩展**，`SSLStream` 包装器 | 不嵌入核心；用户按需 link OpenSSL |
| 5 | **构建系统** | **CMake** + gcc/clang | 用户确认 |
| 6 | **Channel** | **不实现** | 网络库不需要 Channel；Fiber 间同步用 `FiberMutex` + `WaitGroup` 足够；Channel 是应用层原语 |
| 7 | **Fiber 栈** | **固定栈 (128KB 默认, 可配置) + guard page** | 争议最大，单独说明见下方 |
| 8 | **HTTP** | **不实现** | 用户确认：本库只做网络传输层 |

### 7.1 Fiber 栈策略 — 为什么不用 Go 的 growable stack

Go 1.4+ 使用 contiguous stack with copying：栈满时分配 2x 新栈，拷贝旧内容，调整指针。C++ 做这件事极其困难：

1. **指针失效** — C++ 中任何指向栈变量的指针在栈拷贝后悬空，编译器不会帮你调整
2. **汇编上下文** — 手动保存的寄存器中可能含有指向旧栈的地址，无法追踪
3. **复杂度爆炸** — 需要编译期插桩或运行时栈帧遍历，这与"高性能"矛盾

**我们的方案：固定栈 + guard page**

```
Stack layout (128KB = 0x20000):
┌──────────────────────┐  high address
│   Guard Page (4KB)   │  mprotect(PROT_NONE) → SIGSEGV on overflow
├──────────────────────┤
│                      │
│   Usable Stack       │  ~124KB
│   (124KB)            │
│                      │
├──────────────────────┤
│   Stack Bottom       │  low address
└──────────────────────┘
```

- 栈池复用，避免频繁 mmap/munmap
- 128KB 对 IO-bound 协程足够（不需要深递归）
- 需要大栈的场景可配置 `fiber.stack_size = 512KB`
- Guard page 用 `mprotect` 设为不可访问，溢出时直接 SIGSEGV，快速失败，易于定位

> 参考：libco (Tencent)、folly::fibers (Meta)、boost.context 均采用固定栈方案。
