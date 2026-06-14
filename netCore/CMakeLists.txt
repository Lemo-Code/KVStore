cmake_minimum_required(VERSION 3.10)

set(NETCORE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(NETCORE_INCLUDE "${NETCORE_ROOT}/include")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Threads REQUIRED)

function(netcore_apply_perf_flags target_name)
  if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_options(${target_name} PRIVATE -O3 -DNDEBUG)
  endif()
endfunction()

# ── utils ────────────────────────────────────────────────
add_library(netcore_utils_obj OBJECT
  "${NETCORE_ROOT}/src/utils/string_util.cc"
  "${NETCORE_ROOT}/src/utils/thread_util.cc"
  "${NETCORE_ROOT}/src/utils/time_util.cc")
target_include_directories(netcore_utils_obj PUBLIC "${NETCORE_INCLUDE}")
target_compile_features(netcore_utils_obj PUBLIC cxx_std_11)
add_library(netcore_utils STATIC $<TARGET_OBJECTS:netcore_utils_obj>)
add_library(netcore::utils ALIAS netcore_utils)

# ── thread ───────────────────────────────────────────────
add_library(netcore_thread_obj OBJECT
  "${NETCORE_ROOT}/src/thread/semaphore.cc"
  "${NETCORE_ROOT}/src/thread/thread.cc")
target_include_directories(netcore_thread_obj PUBLIC "${NETCORE_INCLUDE}")
target_compile_features(netcore_thread_obj PUBLIC cxx_std_11)
target_link_libraries(netcore_thread_obj PUBLIC netcore_utils Threads::Threads)
add_library(netcore_thread STATIC $<TARGET_OBJECTS:netcore_thread_obj>)
add_library(netcore::thread ALIAS netcore_thread)

# ── memory ───────────────────────────────────────────────
add_library(netcore_memory_obj OBJECT "${NETCORE_ROOT}/src/memory/stack_pool.cc")
target_include_directories(netcore_memory_obj PUBLIC "${NETCORE_INCLUDE}")
target_compile_features(netcore_memory_obj PUBLIC cxx_std_11)
target_link_libraries(netcore_memory_obj PUBLIC Threads::Threads)
add_library(netcore_memory STATIC $<TARGET_OBJECTS:netcore_memory_obj>)
add_library(netcore::memory ALIAS netcore_memory)

# ── fiber ────────────────────────────────────────────────
add_library(netcore_fiber_obj OBJECT
  "${NETCORE_ROOT}/src/fiber/fiber.cc"
  "${NETCORE_ROOT}/src/fiber/fiber_pool.cc"
  "${NETCORE_ROOT}/src/fiber/scheduler.cc"
  "${NETCORE_ROOT}/src/fiber/timer.cc"
  "${NETCORE_ROOT}/src/fiber/timing_wheel.cc")
target_include_directories(netcore_fiber_obj PUBLIC "${NETCORE_INCLUDE}")
target_compile_features(netcore_fiber_obj PUBLIC cxx_std_11)
target_link_libraries(netcore_fiber_obj PUBLIC
  netcore_memory netcore_utils netcore_thread Threads::Threads)
add_library(netcore_fiber STATIC $<TARGET_OBJECTS:netcore_fiber_obj>)
target_link_libraries(netcore_fiber PUBLIC netcore_memory netcore_utils netcore_thread Threads::Threads)
add_library(netcore::fiber ALIAS netcore_fiber)

# ── io（sylar 风格 IOManager）────────────────────────────
add_library(netcore_io_obj OBJECT
  "${NETCORE_ROOT}/src/io/fd_context.cc"
  "${NETCORE_ROOT}/src/io/iomanager.cc"
  "${NETCORE_ROOT}/src/io/hook.cc"
  "${NETCORE_ROOT}/src/io/connect.cc")
target_include_directories(netcore_io_obj PUBLIC "${NETCORE_INCLUDE}")
target_compile_features(netcore_io_obj PUBLIC cxx_std_11)
target_link_libraries(netcore_io_obj PUBLIC
  netcore_fiber netcore_thread netcore_utils Threads::Threads ${CMAKE_DL_LIBS})
add_library(netcore_io STATIC $<TARGET_OBJECTS:netcore_io_obj>)
add_library(netcore::io ALIAS netcore_io)

# ── buffer ───────────────────────────────────────────────
add_library(netcore_buffer_obj OBJECT "${NETCORE_ROOT}/src/buffer/ring_buffer.cc")
target_include_directories(netcore_buffer_obj PUBLIC "${NETCORE_INCLUDE}")
add_library(netcore_buffer STATIC $<TARGET_OBJECTS:netcore_buffer_obj>)
add_library(netcore::buffer ALIAS netcore_buffer)

# ── socket ───────────────────────────────────────────────
add_library(netcore_socket_obj OBJECT
  "${NETCORE_ROOT}/src/socket/address.cc"
  "${NETCORE_ROOT}/src/socket/socket.cc")
target_include_directories(netcore_socket_obj PUBLIC "${NETCORE_INCLUDE}")
target_link_libraries(netcore_socket_obj PUBLIC
  netcore_io netcore_buffer netcore_utils Threads::Threads ${CMAKE_DL_LIBS})
add_library(netcore_socket STATIC $<TARGET_OBJECTS:netcore_socket_obj>)
add_library(netcore::socket ALIAS netcore_socket)

# ── log ──────────────────────────────────────────────────
add_library(netcore_log_obj OBJECT
  "${NETCORE_ROOT}/src/log/level.cc"
  "${NETCORE_ROOT}/src/log/event.cc"
  "${NETCORE_ROOT}/src/log/pattern_layout.cc"
  "${NETCORE_ROOT}/src/log/appender.cc"
  "${NETCORE_ROOT}/src/log/console_appender.cc"
  "${NETCORE_ROOT}/src/log/file_appender.cc"
  "${NETCORE_ROOT}/src/log/log_paths.cc"
  "${NETCORE_ROOT}/src/log/rolling_file_appender.cc"
  "${NETCORE_ROOT}/src/log/async_appender.cc"
  "${NETCORE_ROOT}/src/log/log_runtime.cc"
  "${NETCORE_ROOT}/src/log/appender_registry.cc"
  "${NETCORE_ROOT}/src/log/mdc.cc"
  "${NETCORE_ROOT}/src/log/logger.cc"
  "${NETCORE_ROOT}/src/log/logger_repository.cc")
target_include_directories(netcore_log_obj PUBLIC "${NETCORE_INCLUDE}")
target_compile_features(netcore_log_obj PUBLIC cxx_std_11)
target_link_libraries(netcore_log_obj PUBLIC
  netcore_fiber netcore_utils netcore_thread Threads::Threads)
add_library(netcore_log STATIC $<TARGET_OBJECTS:netcore_log_obj>)
add_library(netcore::log ALIAS netcore_log)

# ── config ───────────────────────────────────────────────
add_library(netcore_config_obj OBJECT
  "${NETCORE_ROOT}/src/config/config_var.cc"
  "${NETCORE_ROOT}/src/config/config_center.cc"
  "${NETCORE_ROOT}/src/config/property_loader.cc"
  "${NETCORE_ROOT}/src/config/log_config.cc"
  "${NETCORE_ROOT}/src/config/config_printer.cc")
target_include_directories(netcore_config_obj PUBLIC "${NETCORE_INCLUDE}")
target_compile_features(netcore_config_obj PUBLIC cxx_std_11)
target_link_libraries(netcore_config_obj PUBLIC netcore_log netcore_utils)
add_library(netcore_config STATIC $<TARGET_OBJECTS:netcore_config_obj>)
add_library(netcore::config ALIAS netcore_config)

foreach(_obj netcore_utils_obj netcore_thread_obj netcore_memory_obj netcore_fiber_obj
             netcore_io_obj netcore_buffer_obj netcore_socket_obj
             netcore_log_obj netcore_config_obj)
  netcore_apply_perf_flags(${_obj})
endforeach()

# ── 聚合库 ───────────────────────────────────────────────
add_library(netcore STATIC
  $<TARGET_OBJECTS:netcore_utils_obj>
  $<TARGET_OBJECTS:netcore_thread_obj>
  $<TARGET_OBJECTS:netcore_memory_obj>
  $<TARGET_OBJECTS:netcore_fiber_obj>
  $<TARGET_OBJECTS:netcore_io_obj>
  $<TARGET_OBJECTS:netcore_buffer_obj>
  $<TARGET_OBJECTS:netcore_socket_obj>
  $<TARGET_OBJECTS:netcore_log_obj>
  $<TARGET_OBJECTS:netcore_config_obj>)
target_include_directories(netcore PUBLIC "${NETCORE_INCLUDE}")
target_link_libraries(netcore PUBLIC
  netcore_config netcore_log netcore_socket netcore_io netcore_fiber
  netcore_memory netcore_buffer netcore_thread netcore_utils
  Threads::Threads ${CMAKE_DL_LIBS})
add_library(netcore::netcore ALIAS netcore)

# ── 性能基准 ─────────────────────────────────────────────
set(netcore_bin "${KVSTORE_BIN_ROOT}/netCore")
file(MAKE_DIRECTORY "${netcore_bin}")

add_executable(bench_echo_server_netcore
  "${CMAKE_SOURCE_DIR}/tests/lemo/socket/bench_echo_server.cc")
target_link_libraries(bench_echo_server_netcore PRIVATE
  netcore_socket netcore_io netcore_fiber netcore_memory netcore_buffer
  netcore_thread netcore_utils)
target_include_directories(bench_echo_server_netcore PRIVATE "${NETCORE_INCLUDE}")
set_target_properties(bench_echo_server_netcore PROPERTIES
  OUTPUT_NAME bench_echo_server
  RUNTIME_OUTPUT_DIRECTORY "${netcore_bin}")
netcore_apply_perf_flags(bench_echo_server_netcore)

add_executable(bench_log_perf_netcore
  "${CMAKE_SOURCE_DIR}/tests/lemo/log/bench_log_perf.cc")
target_link_libraries(bench_log_perf_netcore PRIVATE
  netcore_log netcore_fiber netcore_thread netcore_utils)
target_include_directories(bench_log_perf_netcore PRIVATE "${NETCORE_INCLUDE}")
set_target_properties(bench_log_perf_netcore PROPERTIES
  OUTPUT_NAME bench_log_perf
  RUNTIME_OUTPUT_DIRECTORY "${netcore_bin}")
netcore_apply_perf_flags(bench_log_perf_netcore)

add_executable(bench_scheduler_netcore
  "${CMAKE_SOURCE_DIR}/tests/lemo/fiber/bench_scheduler.cc")
target_link_libraries(bench_scheduler_netcore PRIVATE
  netcore_fiber netcore_memory netcore_thread netcore_utils)
target_include_directories(bench_scheduler_netcore PRIVATE "${NETCORE_INCLUDE}")
set_target_properties(bench_scheduler_netcore PROPERTIES
  OUTPUT_NAME bench_scheduler
  RUNTIME_OUTPUT_DIRECTORY "${netcore_bin}")
netcore_apply_perf_flags(bench_scheduler_netcore)

add_custom_target(bench_echo_server_netcore_run
  COMMAND "${netcore_bin}/bench_echo_server" --quick
  DEPENDS bench_echo_server_netcore
  COMMENT "运行 netCore echo 快速基准")

add_custom_target(bench_echo_server_netcore_perf
  COMMAND "${netcore_bin}/bench_echo_server"
    --mode local --threads 4 --connections 64 --payload 128 --messages 1000
  DEPENDS bench_echo_server_netcore
  COMMENT "运行 netCore echo 性能基准（4T/64C/128B/1000msg）")

add_custom_target(bench_netcore_perf
  COMMAND "${netcore_bin}/bench_echo_server"
    --mode local --threads 4 --connections 64 --payload 128 --messages 1000
  COMMAND "${netcore_bin}/bench_scheduler" --quick
  COMMAND "${netcore_bin}/bench_log_perf"
  DEPENDS bench_echo_server_netcore bench_scheduler_netcore bench_log_perf_netcore
  COMMENT "运行 netCore 全套性能基准")
