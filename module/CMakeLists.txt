# module/ — 分层组件入口
#
# 依赖关系（自底向上）：
#   lstl (memory + container)
#     → storage (lsmtree)
#     → kv_pool（可选注入 lstl 分配器）
#     → net（基础设施：base / runtime / log）

add_subdirectory(lstl)
add_subdirectory(storage)

if(LSTL_BUILD_KV_POOL)
  add_subdirectory(kv_pool)
endif()

if(LSTL_BUILD_NET_LOG)
  add_subdirectory(net)
endif()
