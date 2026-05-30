#ifndef NET_LOG_CONFIG_LOG_CONFIG_BRIDGE_H
#define NET_LOG_CONFIG_LOG_CONFIG_BRIDGE_H

namespace net {

/**
 * @brief 向 ConfigCenter 注册日志相关配置项并绑定变更回调。
 *
 * loadFromYaml* / 包含 log.h 时会自动调用。
 * ConfigCenter::clear() 后会重置，下次加载会重新注册。
 */
void InitLogConfigBridge();

/** ConfigCenter::clear() 时调用，释放桥接侧缓存的 ConfigVar 指针 */
void ResetLogConfigBridge();

}  // namespace net

#endif  // NET_LOG_CONFIG_LOG_CONFIG_BRIDGE_H
