#ifndef NET_LOG_BUILD_CONFIG_H
#define NET_LOG_BUILD_CONFIG_H

/**
 * @file build_config.h
 * @brief 日志模块编译期配置项（可用 -D 覆盖）。
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

/** 异步刷盘 worker 线程名（pthread 最长 15 字符，供 %N 与调试） */
#ifndef NET_LOG_ASYNC_WORKER_NAME
#define NET_LOG_ASYNC_WORKER_NAME "log_async"
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
 * 队列本身无界；该值供 LogConfig 降级策略判断积压是否过多。
 */
#ifndef NET_LOG_ASYNC_SOFT_CAP
#define NET_LOG_ASYNC_SOFT_CAP 4096
#endif

/**
 * 异步过载降级模式：
 *   0 = 不降级
 *   1 = 超过软上限时丢弃新日志
 *   2 = 超过软上限时按 NET_LOG_DEGRADE_SAMPLE_RATE 采样保留
 */
#ifndef NET_LOG_DEGRADE_MODE
#define NET_LOG_DEGRADE_MODE 0
#endif

/**
 * 采样降级保留率：积压超限时每 N 条保留 1 条（仅 degrade_mode=2 生效）。
 * 取值 >= 1，默认 10。
 */
#ifndef NET_LOG_DEGRADE_SAMPLE_RATE
#define NET_LOG_DEGRADE_SAMPLE_RATE 10
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

#endif  // NET_LOG_BUILD_CONFIG_H
