#pragma once

/**
 * @file module.h
 * @brief lemo 日志模块统一入口（含线程 TLS 整合）。
 *
 * 业务代码： #include "lemo/log/module.h"
 *   LEMO_LOG_INFO(LEMO_LOG_ROOT()) << "started";
 */

#include "lemo/log/appender_registry.h"
#include "lemo/log/async_appender.h"
#include "lemo/log/console_appender.h"
#include "lemo/log/file_appender.h"
#include "lemo/log/layout.h"
#include "lemo/log/log.h"
#include "lemo/log/log_paths.h"
#include "lemo/log/log_runtime.h"
#include "lemo/log/logger.h"
#include "lemo/log/logger_repository.h"
#include "lemo/log/pattern_layout.h"
#include "lemo/log/rolling_file_appender.h"
