cmake_minimum_required(VERSION 3.10)
project(KVStore VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# ── 构建开关 ──────────────────────────────────────────────
option(LSTL_BUILD_CONTAINER "构建 LSTL 容器层与测试" ON)
option(LSTL_BUILD_LSM_TREE "构建 storage/lsmtree 与测试" ON)
option(LSTL_BUILD_KV_POOL "构建 kv_pool 多线程内存池与测试" ON)
option(LSTL_BUILD_NET_LOG "构建 net 基础设施（base/runtime/log）与测试" ON)
option(LSTL_BUILD_BENCH "构建 malloc/stl/lstl pool 压测" OFF)
option(LSTL_BUILD_MEMORY_STRESS "构建空间配置压力测试" ON)

# lstl memory 编译选项
option(LSTL_OOM_MODE_CERR "OOM 时 cerr 输出并 abort" OFF)
option(LSTL_USE_MALLOC_ALLOC "全局 alloc 使用 malloc 而非默认 pool" OFF)
option(LSTL_POOL_TRACK_SPAN "pool 启用 Span 追踪与 purge（默认 LIGHT 对齐 SGI）" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(KVStoreHelpers)

enable_testing()

add_subdirectory(module)

if(LSTL_BUILD_BENCH)
  set(bench_bin "${KVSTORE_BIN_ROOT}/lstl/memory")
  file(MAKE_DIRECTORY "${bench_bin}")

  add_executable(bench_alloc_compare
    "${CMAKE_SOURCE_DIR}/tests/lstl/memory/benchmarks/bench_alloc_compare.cc")
  target_include_directories(bench_alloc_compare PRIVATE
    "${CMAKE_SOURCE_DIR}/tests/lstl/memory")
  target_link_libraries(bench_alloc_compare PRIVATE lstl_memory)
  set_target_properties(bench_alloc_compare PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${bench_bin}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${bench_bin}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${bench_bin}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${bench_bin}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${bench_bin}"
  )
  kvstore_apply_target_options(bench_alloc_compare)

  add_custom_target(run_bench_compare
    COMMAND "${bench_bin}/bench_alloc_compare"
    DEPENDS bench_alloc_compare
    COMMENT "运行 SGI vs lstl 单线程二级池压测"
  )

  if(TARGET bench_kv_pool)
    add_custom_target(run_bench_kv_pool
      COMMAND "${KVSTORE_BIN_ROOT}/kv_pool/bench_kv_pool"
      DEPENDS bench_kv_pool
      COMMENT "运行 malloc/lstl/kv_pool 性能压测"
    )
  endif()
endif()

add_custom_target(run_tests
  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
  DEPENDS ${KVSTORE_ALL_TEST_TARGETS}
  COMMENT "运行 KVStore 全部单元测试"
)

add_custom_target(run_tests_lstl
  COMMAND ${CMAKE_CTEST_COMMAND} -R "^lstl\\." --output-on-failure
  DEPENDS ${KVSTORE_ALL_TEST_TARGETS}
  COMMENT "仅运行 lstl 层测试"
)

# 兼容旧 target 名
add_custom_target(run_tests_memory
  COMMAND ${CMAKE_CTEST_COMMAND} -R "^lstl\\.memory\\." --output-on-failure
  DEPENDS ${KVSTORE_ALL_TEST_TARGETS}
  COMMENT "仅运行 lstl/memory 测试"
)
