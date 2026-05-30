#ifndef NET_LOG_LOG_H
#define NET_LOG_LOG_H

/**
 * @file log.h
 * @brief net 日志模块统一入口。
 *
 * 业务代码： #include "log/log.h"
 *   NET_LOG_INFO(NET_LOG_ROOT()) << "started";
 */

#include "common/singleton.h"
#include "common/util.h"
#include "thread/module.h"
#include "log/config/build_config.h"
#include "log/config/log_config.h"
#include "log/config/log_config_bridge.h"
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

#endif  // NET_LOG_LOG_H
