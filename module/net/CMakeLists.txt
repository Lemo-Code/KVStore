set(NET_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

find_package(yaml-cpp REQUIRED)
if(NOT yaml-cpp_FOUND)
  message(FATAL_ERROR "yaml-cpp not found, please install libyaml-cpp-dev")
endif()
message(STATUS "yaml-cpp found: ${yaml-cpp_DIR}")

find_package(Threads REQUIRED)

# ── 逻辑分层（OBJECT 库），最终合并为 net_log ────────────────
# base:    common / config / thread
# runtime: fiber / scheduler
# io:      fdmanager / iomanager / socket / buffer
# log:     异步日志

add_library(net_base_obj OBJECT
  "${NET_ROOT}/common/util.cc"
  "${NET_ROOT}/thread/semaphore.cc"
  "${NET_ROOT}/thread/thread.cc"
  "${NET_ROOT}/config/config_center.cc"
  "${NET_ROOT}/config/config_mgr.cc"
)
target_include_directories(net_base_obj PUBLIC "${NET_ROOT}")
target_compile_features(net_base_obj PUBLIC cxx_std_11)
target_link_libraries(net_base_obj PUBLIC Threads::Threads yaml-cpp)

add_library(net_runtime_obj OBJECT
  "${NET_ROOT}/fiber/fiber.cc"
  "${NET_ROOT}/fiber/scheduler.cc"
  "${NET_ROOT}/fiber/timer.cc"
  "${NET_ROOT}/fiber/timing_wheel.cc"
)
target_link_libraries(net_runtime_obj PUBLIC net_base_obj)

add_library(net_io_obj OBJECT
  "${NET_ROOT}/io/connect.cc"
  "${NET_ROOT}/io/fdmanager.cc"
  "${NET_ROOT}/io/hook.cc"
  "${NET_ROOT}/io/iomanager.cc"
  "${NET_ROOT}/buffer/byte_array.cc"
  "${NET_ROOT}/buffer/ring_buffer.cc"
  "${NET_ROOT}/socket/address.cc"
  "${NET_ROOT}/socket/socket.cc"
  "${NET_ROOT}/socket/stream.cc"
)
target_link_libraries(net_io_obj PUBLIC net_runtime_obj)

add_library(net_log_obj OBJECT
  "${NET_ROOT}/log/level.cc"
  "${NET_ROOT}/log/event.cc"
  "${NET_ROOT}/log/wrap.cc"
  "${NET_ROOT}/log/formatter.cc"
  "${NET_ROOT}/log/file_sink.cc"
  "${NET_ROOT}/log/appender.cc"
  "${NET_ROOT}/log/sink.cc"
  "${NET_ROOT}/log/file_writer.cc"
  "${NET_ROOT}/log/async_sink.cc"
  "${NET_ROOT}/log/config/log_config.cc"
  "${NET_ROOT}/log/config/log_define.cc"
  "${NET_ROOT}/log/config/log_config_bridge.cc"
  "${NET_ROOT}/log/logger.cc"
  "${NET_ROOT}/log/manager.cc"
  "${NET_ROOT}/log/log_paths.cc"
)
target_link_libraries(net_log_obj PUBLIC net_io_obj)

add_library(net_log STATIC
  $<TARGET_OBJECTS:net_base_obj>
  $<TARGET_OBJECTS:net_runtime_obj>
  $<TARGET_OBJECTS:net_io_obj>
  $<TARGET_OBJECTS:net_log_obj>
)
target_include_directories(net_log PUBLIC "${NET_ROOT}")
target_link_libraries(net_log PUBLIC Threads::Threads yaml-cpp dl)
add_library(net::log ALIAS net_log)

set(NET_LOG_TESTS
  test_fiber test_scheduler test_timer test_timer_wheel test_iomanager test_hook
  test_address
  test_ring_buffer
  test_byte_array
  test_thread test_log_thread_integration
  test_singleton_util test_config_center test_lexical_cast test_hot_reload
  test_log_config test_log_config_bridge test_log_level test_log_formatter
  test_log_sync test_log_async test_log_async_boundary test_log_appender
  test_log_full test_log_multi_logger
)

set(NET_LOG_BENCH
  bench_log_async bench_log_sync bench_log_mt bench_log_perf bench_scheduler
  bench_ring_buffer
)

set(net_bin "${KVSTORE_BIN_ROOT}/net")
set(net_log_dir "${net_bin}/log")
file(MAKE_DIRECTORY "${net_bin}")
file(MAKE_DIRECTORY "${net_log_dir}")

kvstore_add_tests(net
  LIBS net_log
  INCLUDES "${CMAKE_SOURCE_DIR}/tests/net"
  TESTS ${NET_LOG_TESTS})

foreach(test_name ${NET_LOG_TESTS})
  set_tests_properties("net.${test_name}" PROPERTIES
    WORKING_DIRECTORY "${net_bin}"
    ENVIRONMENT "NET_LOG_DIR=${net_log_dir}")
endforeach()

foreach(bench_name ${NET_LOG_BENCH})
  add_executable(${bench_name}
    "${CMAKE_SOURCE_DIR}/tests/net/${bench_name}.cc")
  target_link_libraries(${bench_name} PRIVATE net_log)
  target_include_directories(${bench_name} PRIVATE "${CMAKE_SOURCE_DIR}/tests/net")
  set_target_properties(${bench_name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${net_bin}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${net_bin}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${net_bin}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${net_bin}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${net_bin}"
  )
  add_custom_target(${bench_name}_run
    COMMAND "${net_bin}/${bench_name}"
    DEPENDS ${bench_name}
    WORKING_DIRECTORY "${net_bin}"
    COMMENT "Run ${bench_name} (logs -> ${net_log_dir})"
  )
endforeach()

option(NET_LOG_BENCH_TSAN
  "为 bench_log_mt_tsan 启用 ThreadSanitizer（死锁/数据竞争检测）" OFF)

if(NET_LOG_BENCH_TSAN)
  target_compile_options(net_log PUBLIC
    -fsanitize=thread -fno-omit-frame-pointer -g -O1)
  target_link_options(net_log PUBLIC -fsanitize=thread)
  add_custom_target(bench_log_mt_tsan
    COMMAND "${net_bin}/bench_log_mt" --quick
    DEPENDS bench_log_mt
    COMMENT "ThreadSanitizer 死锁/竞争检测: bench_log_mt --quick"
  )
endif()

find_program(VALGRIND_EXECUTABLE valgrind)
if(VALGRIND_EXECUTABLE AND TARGET bench_log_mt)
  add_custom_target(bench_log_mt_helgrind
    COMMAND "${CMAKE_COMMAND}" -E env NET_BENCH_QUICK=1
      "${VALGRIND_EXECUTABLE}"
      --error-exitcode=1
      --tool=helgrind
      --history-level=full
      --conflict-cache-size=1000000
      "$<TARGET_FILE:bench_log_mt>"
      --quick
    DEPENDS bench_log_mt
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Helgrind 死锁检测: bench_log_mt --quick"
  )
endif()

set(NET_LOG_ALL_TEST_TARGETS ${NET_LOG_TESTS} PARENT_SCOPE)
