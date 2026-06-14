#pragma once

/**
 * @file nettycore.h
 * @brief LemoNettyCore 总入口：高性能协程网络栈。
 *
 *   #include "lemo/nettycore.h"
 *   lemo::io::Runtime rt(4, false, "main");
 *   lemo::server::TcpServer server("echo", &rt);
 */

#include "lemo/buffer/module.h"
#include "lemo/fiber/module.h"
#include "lemo/io/module.h"
#include "lemo/memory/module.h"
#include "lemo/nettycore_export.h"
#include "lemo/nettycore_version.h"
#include "lemo/server/module.h"
#include "lemo/socket/module.h"
#include "lemo/thread/module.h"
#include "lemo/utils/utils.h"
