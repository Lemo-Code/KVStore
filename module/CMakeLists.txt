# module/ — 可独立构建的子模块（lstl 为 header-only，在根 CMakeLists 中配置）

if(LSTL_BUILD_KV_POOL)
  add_subdirectory(kv_pool)
endif()

if(LSTL_BUILD_NET_LOG)
  add_subdirectory(net)
endif()
