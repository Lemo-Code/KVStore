#ifndef NET_CONFIG_H
#define NET_CONFIG_H

/**
 * @file config.h
 * @brief net 日志模块编译期配置项。
 *
 * 设计原则（与 Sylar 的关系）：
 *   - Sylar 主要参考其日志 API、格式占位符（%m %p %d 等）与使用习惯。
 *   - 异步实现不照搬 Sylar：固定采用无锁 MPSC 队列，不提供有锁/Ringbuffer 备选。
 *   - 流量过载时走降级策略（丢日志、采样等），而非更换异步后端。
 *
 * 所有宏均支持在编译命令行通过 -D 覆盖。
 */

/** 默认日志级别阈值（0=UNKNOWN, 1=DEBUG, 2=INFO, 3=WARN, 4=ERROR, 5=FATAL） */
#ifndef NET_LOG_DEFAULT_LEVEL
#define NET_LOG_DEFAULT_LEVEL 1
#endif

/** 默认日志格式模式（占位符语义参考 Sylar） */
#ifndef NET_LOG_DEFAULT_PATTERN
#define NET_LOG_DEFAULT_PATTERN \
  "%d{%Y-%m-%d %H:%M:%S}%T%t%T[%p]%T[%c]%T%f:%l%T%m%n"
#endif

/** 异步刷盘线程轮询间隔（毫秒） */
#ifndef NET_LOG_ASYNC_FLUSH_MS
#define NET_LOG_ASYNC_FLUSH_MS 800
#endif

/** 每个文件/stdout 的写缓冲容量（字节，构造时一次分配，运行期不扩容） */
#ifndef NET_LOG_ASYNC_BUF_BYTES
#define NET_LOG_ASYNC_BUF_BYTES (256u * 1024u)
#endif

/** 写缓冲达到该阈值时触发 write（应 <= NET_LOG_ASYNC_BUF_BYTES） */
#ifndef NET_LOG_ASYNC_FLUSH_THRESHOLD
#define NET_LOG_ASYNC_FLUSH_THRESHOLD (64u * 1024u)
#endif

/**
 * 无锁队列软上限（条数）。
 * 当前队列本身无界；该值供降级逻辑判断“积压是否过多”，后续可启用 NET_LOG_DEGRADE_MODE。
 */
#ifndef NET_LOG_ASYNC_SOFT_CAP
#define NET_LOG_ASYNC_SOFT_CAP 4096
#endif

/**
 * 异步过载降级模式：0=不降级，1=超过软上限时丢弃新日志。
 * 默认 0；流量压力大时可改为 1 或后续扩展采样模式。
 */
#ifndef NET_LOG_DEGRADE_MODE
#define NET_LOG_DEGRADE_MODE 0
#endif

/** 文件 Appender 检测文件是否被删除的间隔（秒） */
#ifndef NET_LOG_FILE_REOPEN_SEC
#define NET_LOG_FILE_REOPEN_SEC 1
#endif

/** 轮转文件默认单文件上限（字节），RollingFileLogAppender 未显式指定时使用 */
#ifndef NET_LOG_ROLL_DEFAULT_MAX_BYTES
#define NET_LOG_ROLL_DEFAULT_MAX_BYTES (100u * 1024u * 1024u)
#endif

/** 轮转保留的历史文件个数（含 .1 .2 ... 链） */
#ifndef NET_LOG_ROLL_DEFAULT_MAX_FILES
#define NET_LOG_ROLL_DEFAULT_MAX_FILES 10
#endif

#endif  // NET_CONFIG_H
