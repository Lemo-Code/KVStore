#ifndef NET_NET_H
#define NET_NET_H

/**
 * @file net.h
 * @brief net 模块总入口：线程 + 日志 + 配置中心。
 *
 * 典型用法：
 *   #include "net.h"
 *   net::Thread::SetName("main");
 *   NET_LOG_INFO(NET_LOG_ROOT()) << "started";
 */

#include "config/config_mgr.h"
#include "buffer/module.h"
#include "fiber/module.h"
#include "io/module.h"
#include "log/log.h"
#include "socket/module.h"
#include "thread/module.h"

#endif  // NET_NET_H
