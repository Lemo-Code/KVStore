set(KV_POOL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(kv_pool STATIC
  "${KV_POOL_ROOT}/memory/pool.cc"
)
target_include_directories(kv_pool PUBLIC "${KV_POOL_ROOT}")
target_compile_features(kv_pool PUBLIC cxx_std_11)

set(KV_POOL_TESTS
  test_pool_mt
  test_cross_thread
)

set(kv_pool_bin "${LSTL_BIN_ROOT}/kv_pool")
file(MAKE_DIRECTORY "${kv_pool_bin}")

foreach(test_name ${KV_POOL_TESTS})
  add_executable(${test_name}
    "${CMAKE_SOURCE_DIR}/tests/kv_pool/${test_name}.cc")
  target_link_libraries(${test_name} PRIVATE kv_pool)
  find_package(Threads REQUIRED)
  target_link_libraries(${test_name} PRIVATE Threads::Threads)
  set_target_properties(${test_name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${kv_pool_bin}"
  )
  add_test(NAME "kv_pool.${test_name}" COMMAND "${kv_pool_bin}/${test_name}")
  list(APPEND KV_POOL_ALL_TEST_TARGETS ${test_name})
endforeach()

set(KV_POOL_ALL_TEST_TARGETS "${KV_POOL_ALL_TEST_TARGETS}" PARENT_SCOPE)

if(LSTL_BUILD_BENCH)
  add_executable(bench_kv_pool
    "${CMAKE_SOURCE_DIR}/tests/kv_pool/benchmarks/bench_kv_pool.cc")
  target_link_libraries(bench_kv_pool PRIVATE kv_pool lstl_memory)
  target_include_directories(bench_kv_pool PRIVATE
    "${KV_POOL_ROOT}"
    "${CMAKE_SOURCE_DIR}/module/lstl")
  find_package(Threads REQUIRED)
  target_link_libraries(bench_kv_pool PRIVATE Threads::Threads)
  set_target_properties(bench_kv_pool PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${kv_pool_bin}"
  )
endif()
