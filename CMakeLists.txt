cmake_minimum_required(VERSION 3.10)
project(LSTL VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

option(LSTL_OOM_MODE_CERR "OOM 时 cerr 输出并 abort" OFF)
option(LSTL_USE_MALLOC_ALLOC "全局 alloc 使用 malloc 而非默认 pool" OFF)
option(LSTL_POOL_TRACK_SPAN "pool 启用 Span 追踪与 purge（默认 LIGHT 对齐 SGI）" OFF)
option(LSTL_BUILD_KV_POOL "构建多线程 kv_pool 组件与测试" ON)
option(LSTL_BUILD_NET_LOG "构建 net 日志模块与测试" ON)
option(LSTL_BUILD_BENCH "构建 malloc/stl/lstl pool 压测" OFF)
option(LSTL_BUILD_CONTAINER "构建 LSTL 容器层与测试" ON)

# 空间配置 + 容器：header-only
set(LSTL_MEMORY_ROOT "${CMAKE_SOURCE_DIR}/module/lstl/memory")
set(LSTL_CONTAINER_ROOT "${CMAKE_SOURCE_DIR}/module/lstl/container")

add_library(lstl_memory INTERFACE)
target_include_directories(lstl_memory INTERFACE
  ${LSTL_MEMORY_ROOT}
  ${LSTL_CONTAINER_ROOT})

if(LSTL_OOM_MODE_CERR)
  target_compile_definitions(lstl_memory INTERFACE LSTL_OOM_MODE_CERR)
endif()

if(LSTL_USE_MALLOC_ALLOC)
  target_compile_definitions(lstl_memory INTERFACE LSTL_USE_MALLOC_ALLOC=1)
endif()

if(LSTL_POOL_TRACK_SPAN)
  target_compile_definitions(lstl_memory INTERFACE LSTL_POOL_LIGHT=0)
endif()

option(LSTL_BUILD_MEMORY_STRESS "构建空间配置压力测试" ON)

set(LSTL_MEMORY_TESTS
  test_alloc
  test_construct
  test_uninitialized
  test_allocator
  test_temporary_buffer
  test_type_traits
  test_oom_policy
  test_pool_single
)

if(LSTL_BUILD_MEMORY_STRESS)
  list(APPEND LSTL_MEMORY_TESTS test_memory_stress)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(LSTLHelpers)

enable_testing()

lstl_add_module_tests(memory TESTS ${LSTL_MEMORY_TESTS})

if(LSTL_BUILD_CONTAINER)
  lstl_add_module_tests(container TESTS test_vector)
endif()

if(LSTL_BUILD_KV_POOL OR LSTL_BUILD_NET_LOG)
  add_subdirectory(module)
  if(LSTL_BUILD_KV_POOL)
    list(APPEND LSTL_ALL_TEST_TARGETS ${KV_POOL_ALL_TEST_TARGETS})
  endif()
  if(LSTL_BUILD_NET_LOG)
    list(APPEND LSTL_ALL_TEST_TARGETS ${NET_LOG_ALL_TEST_TARGETS})
  endif()
  set(LSTL_ALL_TEST_TARGETS "${LSTL_ALL_TEST_TARGETS}" CACHE INTERNAL "" FORCE)
endif()

if(LSTL_BUILD_BENCH)
  set(bench_bin "${LSTL_BIN_ROOT}/memory")
  file(MAKE_DIRECTORY "${bench_bin}")

  add_executable(bench_alloc_compare
    "${CMAKE_SOURCE_DIR}/tests/memory/benchmarks/bench_alloc_compare.cc")
  target_include_directories(bench_alloc_compare PRIVATE
    "${CMAKE_SOURCE_DIR}/tests/memory")
  set_target_properties(bench_alloc_compare PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${bench_bin}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${bench_bin}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${bench_bin}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${bench_bin}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${bench_bin}"
  )
  lstl_apply_target_options(bench_alloc_compare)

  add_custom_target(run_bench_compare
    COMMAND "${bench_bin}/bench_alloc_compare"
    DEPENDS bench_alloc_compare
    COMMENT "运行 SGI vs lstl 单线程二级池压测"
  )

  set(kv_pool_bench_bin "${LSTL_BIN_ROOT}/kv_pool")
  add_custom_target(run_bench_kv_pool
    COMMAND "${kv_pool_bench_bin}/bench_kv_pool"
    DEPENDS bench_kv_pool
    COMMENT "运行 malloc/lstl/kv_pool 性能压测"
  )
endif()

add_custom_target(run_tests
  COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
  DEPENDS ${LSTL_ALL_TEST_TARGETS}
  COMMENT "运行 LSTL 空间配置模块单元测试"
)

add_custom_target(run_tests_memory
  COMMAND ${CMAKE_CTEST_COMMAND} -R "^memory\\." --output-on-failure
  DEPENDS ${LSTL_ALL_TEST_TARGETS}
  COMMENT "仅运行 memory 模块测试"
)
