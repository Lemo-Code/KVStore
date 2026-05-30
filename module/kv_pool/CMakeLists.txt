set(KV_POOL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(kv_pool STATIC
  "${KV_POOL_ROOT}/memory/pool.cc"
)
add_library(kv_pool::kv_pool ALIAS kv_pool)
target_include_directories(kv_pool PUBLIC "${KV_POOL_ROOT}")
target_compile_features(kv_pool PUBLIC cxx_std_11)

kvstore_add_tests(kv_pool
  LIBS kv_pool
  INCLUDES "${CMAKE_SOURCE_DIR}/tests/kv_pool"
  TESTS test_pool_mt test_cross_thread)

set(KV_POOL_ALL_TEST_TARGETS test_pool_mt test_cross_thread PARENT_SCOPE)

if(LSTL_BUILD_BENCH)
  set(kv_pool_bin "${KVSTORE_BIN_ROOT}/kv_pool")
  add_executable(bench_kv_pool
    "${CMAKE_SOURCE_DIR}/tests/kv_pool/benchmarks/bench_kv_pool.cc")
  target_link_libraries(bench_kv_pool PRIVATE kv_pool lstl_memory)
  target_include_directories(bench_kv_pool PRIVATE
    "${KV_POOL_ROOT}"
    "${CMAKE_SOURCE_DIR}/module/lstl/memory")
  find_package(Threads REQUIRED)
  target_link_libraries(bench_kv_pool PRIVATE Threads::Threads)
  set_target_properties(bench_kv_pool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${kv_pool_bin}"
  )
endif()
