#ifndef NET_LOG_H
#define NET_LOG_H

/**
 * @file log.h
 * @brief net 日志模块统一入口。
 *
 * Sylar 仅作 API/格式参考；异步固定无锁 MPSC + 可配置降级。
 * 业务代码： #include "log.h"
 *   NET_LOG_INFO(NET_LOG_ROOT()) << "started";
 */

#include "config.h"
#include "singleton.h"
#include "util.h"
#include "log/level.h"
#include "log/event.h"
#include "log/wrap.h"
#include "log/formatter.h"
#include "log/sink.h"
#include "log/appender.h"
#include "log/async_sink.h"
#include "log/logger.h"
#include "log/manager.h"
#include "log/macros.h"

#endif  // NET_LOG_H
