#pragma once

/**
 * @file nettycore.h
 * @brief LemoNettyCore 总入口：高性能协程网络栈（核心层，无 log/config）。
 *
 *   #include "lemo/nettycore.h"
 *   lemo::io::IOManager iom(4, true, "main");
 */

#include "lemo/buffer/module.h"
#include "lemo/fiber/module.h"
#include "lemo/io/module.h"
#include "lemo/memory/module.h"
#include "lemo/socket/module.h"
#include "lemo/thread/module.h"
#include "lemo/utils/utils.h"
