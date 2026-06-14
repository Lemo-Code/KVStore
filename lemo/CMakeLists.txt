cmake_minimum_required(VERSION 3.10)

set(LEMO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(LEMO_INCLUDE "${LEMO_ROOT}/include")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Threads REQUIRED)

function(lemo_apply_perf_flags target_name)
  if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_options(${target_name} PRIVATE -O3 -DNDEBUG)
  endif()
endfunction()

# ── memory（仅协程 ucontext 栈池，不依赖 kv_pool）────────
add_library(lemo_memory_obj OBJECT
  "${LEMO_ROOT}/src/memory/stack_pool.cc"
)
target_include_directories(lemo_memory_obj PUBLIC "${LEMO_INCLUDE}")
target_compile_features(lemo_memory_obj PUBLIC cxx_std_11)
target_link_libraries(lemo_memory_obj PUBLIC Threads::Threads)

add_library(lemo_memory STATIC $<TARGET_OBJECTS:lemo_memory_obj>)
target_include_directories(lemo_memory PUBLIC "${LEMO_INCLUDE}")
target_link_libraries(lemo_memory PUBLIC Threads::Threads)
add_library(lemo::memory ALIAS lemo_memory)

# ── utils ────────────────────────────────────────────────
add_library(lemo_utils_obj OBJECT
  "${LEMO_ROOT}/src/utils/string_util.cc"
  "${LEMO_ROOT}/src/utils/thread_util.cc"
  "${LEMO_ROOT}/src/utils/time_util.cc"
)
target_include_directories(lemo_utils_obj PUBLIC "${LEMO_INCLUDE}")
target_compile_features(lemo_utils_obj PUBLIC cxx_std_11)
target_link_libraries(lemo_utils_obj PUBLIC Threads::Threads)

add_library(lemo_utils STATIC $<TARGET_OBJECTS:lemo_utils_obj>)
target_include_directories(lemo_utils PUBLIC "${LEMO_INCLUDE}")
target_link_libraries(lemo_utils PUBLIC Threads::Threads)
add_library(lemo::utils ALIAS lemo_utils)

# ── thread ───────────────────────────────────────────────
add_library(lemo_thread_obj OBJECT
  "${LEMO_ROOT}/src/thread/semaphore.cc"
  "${LEMO_ROOT}/src/thread/thread.cc"
)
target_include_directories(lemo_thread_obj PUBLIC "${LEMO_INCLUDE}")
target_compile_features(lemo_thread_obj PUBLIC cxx_std_11)
target_link_libraries(lemo_thread_obj PUBLIC lemo_utils Threads::Threads)

add_library(lemo_thread STATIC $<TARGET_OBJECTS:lemo_thread_obj>)
target_include_directories(lemo_thread PUBLIC "${LEMO_INCLUDE}")
target_link_libraries(lemo_thread PUBLIC lemo_utils Threads::Threads)
add_library(lemo::thread ALIAS lemo_thread)

# ── fiber ────────────────────────────────────────────────
add_library(lemo_fiber_obj OBJECT
  "${LEMO_ROOT}/src/fiber/fiber.cc"
  "${LEMO_ROOT}/src/fiber/fiber_pool.cc"
  "${LEMO_ROOT}/src/fiber/scheduler.cc"
  "${LEMO_ROOT}/src/fiber/timer.cc"
  "${LEMO_ROOT}/src/fiber/timing_wheel.cc"
)
target_include_directories(lemo_fiber_obj PUBLIC "${LEMO_INCLUDE}")
target_compile_features(lemo_fiber_obj PUBLIC cxx_std_11)
target_link_libraries(lemo_fiber_obj PUBLIC lemo_memory lemo_utils lemo_thread Threads::Threads)

add_library(lemo_fiber STATIC $<TARGET_OBJECTS:lemo_fiber_obj>)
target_include_directories(lemo_fiber PUBLIC "${LEMO_INCLUDE}")
target_link_libraries(lemo_fiber PUBLIC lemo_memory lemo_utils lemo_thread Threads::Threads)
add_library(lemo::fiber ALIAS lemo_fiber)

# ── io ───────────────────────────────────────────────────
add_library(lemo_io_obj OBJECT
  "${LEMO_ROOT}/src/io/reactor.cc"
  "${LEMO_ROOT}/src/io/iomanager.cc"
  "${LEMO_ROOT}/src/io/hook.cc"
  "${LEMO_ROOT}/src/io/connect.cc"
  "${LEMO_ROOT}/src/io/runtime.cc"
)
target_include_directories(lemo_io_obj PUBLIC "${LEMO_INCLUDE}")
target_compile_features(lemo_io_obj PUBLIC cxx_std_11)
target_link_libraries(lemo_io_obj PUBLIC lemo_fiber lemo_thread lemo_utils Threads::Threads ${CMAKE_DL_LIBS})

add_library(lemo_io STATIC $<TARGET_OBJECTS:lemo_io_obj>)
target_include_directories(lemo_io PUBLIC "${LEMO_INCLUDE}")
target_link_libraries(lemo_io PUBLIC lemo_fiber lemo_thread lemo_utils Threads::Threads ${CMAKE_DL_LIBS})
add_library(lemo::io ALIAS lemo_io)

# ── buffer ───────────────────────────────────────────────
add_library(lemo_buffer_obj OBJECT
  "${LEMO_ROOT}/src/buffer/ring_buffer.cc"
)
target_include_directories(lemo_buffer_obj PUBLIC "${LEMO_INCLUDE}")
target_compile_features(lemo_buffer_obj PUBLIC cxx_std_11)
target_link_libraries(lemo_buffer_obj PUBLIC lemo_utils Threads::Threads)

add_library(lemo_buffer STATIC $<TARGET_OBJECTS:lemo_buffer_obj>)
target_include_directories(lemo_buffer PUBLIC "${LEMO_INCLUDE}")
target_link_libraries(lemo_buffer PUBLIC lemo_utils Threads::Threads)
add_library(lemo::buffer ALIAS lemo_buffer)

# ── socket ───────────────────────────────────────────────
add_library(lemo_socket_obj OBJECT
  "${LEMO_ROOT}/src/socket/address.cc"
  "${LEMO_ROOT}/src/socket/socket.cc"
)
target_include_directories(lemo_socket_obj PUBLIC "${LEMO_INCLUDE}")
target_compile_features(lemo_socket_obj PUBLIC cxx_std_11)
target_link_libraries(lemo_socket_obj PUBLIC lemo_io lemo_buffer lemo_utils Threads::Threads ${CMAKE_DL_LIBS})

add_library(lemo_socket STATIC $<TARGET_OBJECTS:lemo_socket_obj>)
target_include_directories(lemo_socket PUBLIC "${LEMO_INCLUDE}")
target_link_libraries(lemo_socket PUBLIC lemo_io lemo_buffer lemo_utils Threads::Threads ${CMAKE_DL_LIBS})
add_library(lemo::socket ALIAS lemo_socket)

# ── log ──────────────────────────────────────────────────
add_library(lemo_log_obj OBJECT
  "${LEMO_ROOT}/src/log/level.cc"
  "${LEMO_ROOT}/src/log/event.cc"
  "${LEMO_ROOT}/src/log/pattern_layout.cc"
  "${LEMO_ROOT}/src/log/appender.cc"
  "${LEMO_ROOT}/src/log/console_appender.cc"
  "${LEMO_ROOT}/src/log/file_appender.cc"
  "${LEMO_ROOT}/src/log/log_paths.cc"
  "${LEMO_ROOT}/src/log/rolling_file_appender.cc"
  "${LEMO_ROOT}/src/log/async_appender.cc"
  "${LEMO_ROOT}/src/log/log_runtime.cc"
  "${LEMO_ROOT}/src/log/appender_registry.cc"
  "${LEMO_ROOT}/src/log/mdc.cc"
  "${LEMO_ROOT}/src/log/logger.cc"
  "${LEMO_ROOT}/src/log/logger_repository.cc"
)
target_include_directories(lemo_log_obj PUBLIC "${LEMO_INCLUDE}")
target_compile_features(lemo_log_obj PUBLIC cxx_std_11)
target_link_libraries(lemo_log_obj PUBLIC lemo_utils lemo_thread lemo_fiber Threads::Threads)

add_library(lemo_log STATIC $<TARGET_OBJECTS:lemo_log_obj>)
target_include_directories(lemo_log PUBLIC "${LEMO_INCLUDE}")
target_link_libraries(lemo_log PUBLIC lemo_utils lemo_thread lemo_fiber Threads::Threads)
add_library(lemo::log ALIAS lemo_log)

# ── config ───────────────────────────────────────────────
add_library(lemo_config_obj OBJECT
  "${LEMO_ROOT}/src/config/config_var.cc"
  "${LEMO_ROOT}/src/config/config_center.cc"
  "${LEMO_ROOT}/src/config/property_loader.cc"
  "${LEMO_ROOT}/src/config/log_config.cc"
  "${LEMO_ROOT}/src/config/config_printer.cc"
)
target_include_directories(lemo_config_obj PUBLIC "${LEMO_INCLUDE}")
target_compile_features(lemo_config_obj PUBLIC cxx_std_11)
target_link_libraries(lemo_config_obj PUBLIC lemo_utils lemo_log)

add_library(lemo_config STATIC $<TARGET_OBJECTS:lemo_config_obj>)
target_include_directories(lemo_config PUBLIC "${LEMO_INCLUDE}")
target_link_libraries(lemo_config PUBLIC lemo_utils lemo_log)
add_library(lemo::config ALIAS lemo_config)

foreach(_lemo_obj IN ITEMS lemo_memory_obj lemo_utils_obj lemo_thread_obj lemo_fiber_obj
                        lemo_io_obj lemo_buffer_obj lemo_socket_obj lemo_log_obj lemo_config_obj)
  lemo_apply_perf_flags(${_lemo_obj})
endforeach()

# ── tests ────────────────────────────────────────────────
set(LEMO_UTILS_TESTS test_utils_string test_utils_lexical_cast test_utils_thread)
set(LEMO_THREAD_TESTS test_thread test_log_thread_integration)
set(LEMO_FIBER_TESTS
  test_stack_pool
  test_fiber
  test_scheduler
  test_timer
  test_timer_wheel
  test_log_fiber_integration
  test_fiber_sleep_iomanager
)
set(LEMO_IO_TESTS test_iomanager test_hook)
set(LEMO_BUFFER_TESTS test_ring_buffer)
set(LEMO_SOCKET_TESTS test_address test_echo_server)
set(LEMO_CONFIG_TESTS
  test_config_property
  test_config_center
  test_log_config
  test_config_full
  test_config_log_integration
)
set(LEMO_LOG_TESTS
  test_log_level
  test_log_hierarchy
  test_log_formatter
  test_log_additivity
  test_log_mdc
  test_log_registry
  test_log_rolling
  test_log_async
  test_log_file_path
  test_log_multithread
)

kvstore_add_tests(lemo/utils
  LIBS lemo_utils
  INCLUDES "${LEMO_INCLUDE}"
  TESTS ${LEMO_UTILS_TESTS}
)

kvstore_add_tests(lemo/thread
  LIBS lemo_thread lemo_log lemo_fiber lemo_utils
  INCLUDES "${LEMO_INCLUDE}"
  TESTS ${LEMO_THREAD_TESTS}
)

kvstore_add_tests(lemo/fiber
  LIBS lemo_fiber lemo_memory lemo_io lemo_thread lemo_log lemo_utils
  INCLUDES "${LEMO_INCLUDE}" "${CMAKE_SOURCE_DIR}/tests/lemo/fiber"
  TESTS ${LEMO_FIBER_TESTS}
)

kvstore_add_tests(lemo/io
  LIBS lemo_io lemo_fiber lemo_thread lemo_utils
  INCLUDES "${LEMO_INCLUDE}"
  TESTS ${LEMO_IO_TESTS}
)

kvstore_add_tests(lemo/buffer
  LIBS lemo_buffer lemo_utils
  INCLUDES "${LEMO_INCLUDE}" "${CMAKE_SOURCE_DIR}/tests/lemo/io"
  TESTS ${LEMO_BUFFER_TESTS}
)

kvstore_add_tests(lemo/socket
  LIBS lemo_socket lemo_io lemo_fiber lemo_buffer lemo_thread lemo_utils
  INCLUDES "${LEMO_INCLUDE}" "${CMAKE_SOURCE_DIR}/tests/lemo/io"
  TESTS ${LEMO_SOCKET_TESTS}
)

add_custom_target(run_tests_lemo_socket
  COMMAND ${CMAKE_CTEST_COMMAND} -R "^lemo\\.socket\\." --output-on-failure
  DEPENDS ${LEMO_SOCKET_TESTS}
  COMMENT "仅运行 lemo socket 模块测试"
)

add_custom_target(run_tests_lemo_io
  COMMAND ${CMAKE_CTEST_COMMAND} -R "^lemo\\.io\\." --output-on-failure
  DEPENDS ${LEMO_IO_TESTS}
  COMMENT "仅运行 lemo io 模块测试"
)

add_custom_target(run_tests_lemo_fiber
  COMMAND ${CMAKE_CTEST_COMMAND} -R "^lemo\\.fiber\\." --output-on-failure
  DEPENDS ${LEMO_FIBER_TESTS}
  COMMENT "仅运行 lemo fiber 模块测试"
)

kvstore_add_tests(lemo/config
  LIBS lemo_config lemo_log lemo_thread lemo_utils
  INCLUDES "${LEMO_INCLUDE}"
  TESTS ${LEMO_CONFIG_TESTS}
)

target_compile_definitions(test_config_full PRIVATE
  LEMO_TEST_CONF="${CMAKE_SOURCE_DIR}/tests/lemo/config/fixtures/lemo_test.conf")

target_compile_definitions(test_config_log_integration PRIVATE
  LEMO_INTEGRATION_CONF="${CMAKE_SOURCE_DIR}/tests/lemo/config/fixtures/lemo_integration.conf")

add_custom_target(run_tests_lemo_config
  COMMAND ${CMAKE_CTEST_COMMAND} -R "^lemo\\.config\\." --output-on-failure
  DEPENDS ${LEMO_CONFIG_TESTS}
  COMMENT "仅运行 lemo config 模块测试"
)

set(lemo_config_bin "${KVSTORE_BIN_ROOT}/lemo/config")
file(MAKE_DIRECTORY "${lemo_config_bin}")

add_executable(config_demo
  "${CMAKE_SOURCE_DIR}/tests/lemo/config/config_demo.cc")
target_link_libraries(config_demo PRIVATE lemo_config lemo_log lemo_utils)
target_include_directories(config_demo PRIVATE "${LEMO_INCLUDE}")
target_compile_definitions(config_demo PRIVATE
  LEMO_DEFAULT_CONF="${CMAKE_SOURCE_DIR}/tests/lemo/config/fixtures/lemo_test.conf")
set_target_properties(config_demo PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${lemo_config_bin}"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${lemo_config_bin}"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${lemo_config_bin}"
  RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${lemo_config_bin}"
  RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${lemo_config_bin}"
)
add_custom_target(config_demo_run
  COMMAND "${lemo_config_bin}/config_demo"
  DEPENDS config_demo
  COMMENT "运行 lemo 配置演示（带完整输出）"
)

kvstore_add_tests(lemo/log
  LIBS lemo_log lemo_fiber lemo_thread lemo_utils
  INCLUDES "${LEMO_INCLUDE}"
  TESTS ${LEMO_LOG_TESTS}
)

add_custom_target(run_tests_lemo
  COMMAND ${CMAKE_CTEST_COMMAND} -R "^lemo\\." --output-on-failure
  DEPENDS ${LEMO_UTILS_TESTS} ${LEMO_THREAD_TESTS} ${LEMO_FIBER_TESTS} ${LEMO_IO_TESTS} ${LEMO_BUFFER_TESTS} ${LEMO_SOCKET_TESTS} ${LEMO_CONFIG_TESTS} ${LEMO_LOG_TESTS}
  COMMENT "仅运行 lemo 层测试"
)

set(lemo_bin "${KVSTORE_BIN_ROOT}/lemo/log")
file(MAKE_DIRECTORY "${lemo_bin}")

add_executable(bench_log_perf
  "${CMAKE_SOURCE_DIR}/tests/lemo/log/bench_log_perf.cc")
target_link_libraries(bench_log_perf PRIVATE lemo_log lemo_thread lemo_utils)
target_include_directories(bench_log_perf PRIVATE "${LEMO_INCLUDE}")
set_target_properties(bench_log_perf PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${lemo_bin}"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${lemo_bin}"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${lemo_bin}"
  RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${lemo_bin}"
  RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${lemo_bin}"
)
add_custom_target(bench_log_perf_run
  COMMAND "${lemo_bin}/bench_log_perf"
  DEPENDS bench_log_perf
  COMMENT "运行 lemo 日志性能基准"
)

add_executable(bench_log_mt_perf
  "${CMAKE_SOURCE_DIR}/tests/lemo/log/bench_log_mt_perf.cc")
target_link_libraries(bench_log_mt_perf PRIVATE lemo_log lemo_thread lemo_utils)
target_include_directories(bench_log_mt_perf PRIVATE "${LEMO_INCLUDE}")
set_target_properties(bench_log_mt_perf PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${lemo_bin}"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${lemo_bin}"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${lemo_bin}"
  RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${lemo_bin}"
  RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${lemo_bin}"
)
add_custom_target(bench_log_mt_perf_run
  COMMAND "${lemo_bin}/bench_log_mt_perf"
  DEPENDS bench_log_mt_perf
  COMMENT "运行 lemo 多线程日志性能基准"
)

set(lemo_fiber_bin "${KVSTORE_BIN_ROOT}/lemo/fiber")
file(MAKE_DIRECTORY "${lemo_fiber_bin}")

add_executable(bench_scheduler
  "${CMAKE_SOURCE_DIR}/tests/lemo/fiber/bench_scheduler.cc")
target_link_libraries(bench_scheduler PRIVATE lemo_fiber lemo_thread lemo_utils)
target_include_directories(bench_scheduler PRIVATE "${LEMO_INCLUDE}")
set_target_properties(bench_scheduler PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${lemo_fiber_bin}"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${lemo_fiber_bin}"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${lemo_fiber_bin}"
  RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${lemo_fiber_bin}"
  RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${lemo_fiber_bin}"
)
add_custom_target(bench_scheduler_run
  COMMAND "${lemo_fiber_bin}/bench_scheduler" --quick
  DEPENDS bench_scheduler
  COMMENT "运行 lemo 调度器性能基准"
)

set(lemo_socket_bin "${KVSTORE_BIN_ROOT}/lemo/socket")
file(MAKE_DIRECTORY "${lemo_socket_bin}")

add_executable(bench_echo_server
  "${CMAKE_SOURCE_DIR}/tests/lemo/socket/bench_echo_server.cc")
target_link_libraries(bench_echo_server PRIVATE lemo_socket lemo_io lemo_fiber lemo_buffer lemo_thread lemo_utils)
target_include_directories(bench_echo_server PRIVATE "${LEMO_INCLUDE}")
set_target_properties(bench_echo_server PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${lemo_socket_bin}"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG "${lemo_socket_bin}"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${lemo_socket_bin}"
  RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${lemo_socket_bin}"
  RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${lemo_socket_bin}"
)
add_custom_target(bench_echo_server_run
  COMMAND "${lemo_socket_bin}/bench_echo_server" --quick
  DEPENDS bench_echo_server
  COMMENT "运行 lemo echo 服务器性能基准"
)
