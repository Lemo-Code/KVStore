#ifndef LSTL_CONFIG_H
#define LSTL_CONFIG_H

// 默认对齐（字节）
#ifndef LSTL_ALIGN
#define LSTL_ALIGN 8
#endif

// 小块池化上限（SGI 二级配置器：8~128B，16 档自由链表）
#ifndef LSTL_POOL_MAX_BYTES
#define LSTL_POOL_MAX_BYTES 128
#endif

// 大块池化上限（超过则直接 malloc）
#ifndef LSTL_LARGE_MAX
#define LSTL_LARGE_MAX 32768
#endif

// 小块自由链表档位数
#ifndef LSTL_FREELISTS
#define LSTL_FREELISTS 16
#endif

// Span 页大小（4KB，仅 LSTL_POOL_LIGHT=0 时使用）
#ifndef LSTL_PAGE_SHIFT
#define LSTL_PAGE_SHIFT 12
#endif

#ifndef LSTL_PAGE_SIZE
#define LSTL_PAGE_SIZE (1u << LSTL_PAGE_SHIFT)
#endif

// 从 chunk refill 到自由链表的默认批量块数
#ifndef LSTL_POOL_REFILL_BATCH
#define LSTL_POOL_REFILL_BATCH 20
#endif

// 1=SGI 热路径（默认）：无 Span 反查；0=启用 Span 追踪与 purge
#ifndef LSTL_POOL_LIGHT
#define LSTL_POOL_LIGHT 1
#endif

// temporary_buffer 单次请求上限参考（字节）
#ifndef LSTL_MAX_BYTES
#define LSTL_MAX_BYTES 32768
#endif

// 置 1 时全局 alloc 退化为 malloc 一级配置器
#ifndef LSTL_USE_MALLOC_ALLOC
#define LSTL_USE_MALLOC_ALLOC 0
#endif

#endif  // LSTL_CONFIG_H
