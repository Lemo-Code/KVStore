cmake_minimum_required(VERSION 3.10)

set(NETTYCORE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(NETTYCORE_INCLUDE "${NETTYCORE_ROOT}/include")

option(NETTYCORE_BUILD_SHARED "构建 lemo_nettycore 动态库" ON)
option(NETTYCORE_BUILD_CONFIG "构建 config + log 伴生库" ON)

set(LEMO_SRC_ROOT "${CMAKE_SOURCE_DIR}/lemo")
set(LEMO_SRC_INCLUDE "${LEMO_SRC_ROOT}/include")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Threads REQUIRED)
include(GNUInstallDirs)

function(nettycore_apply_perf_flags target_name)
  if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_options(${target_name} PRIVATE -O3 -DNDEBUG)
  endif()
endfunction()

# ── utils ────────────────────────────────────────────────
add_library(nettycore_utils_obj OBJECT
  "${NETTYCORE_ROOT}/src/utils/string_util.cc"
  "${NETTYCORE_ROOT}/src/utils/thread_util.cc"
  "${NETTYCORE_ROOT}/src/utils/time_util.cc")
target_include_directories(nettycore_utils_obj PUBLIC "${NETTYCORE_INCLUDE}")
target_compile_features(nettycore_utils_obj PUBLIC cxx_std_11)
add_library(nettycore_utils STATIC $<TARGET_OBJECTS:nettycore_utils_obj>)
add_library(nettycore::utils ALIAS nettycore_utils)

# ── thread ───────────────────────────────────────────────
add_library(nettycore_thread_obj OBJECT
  "${NETTYCORE_ROOT}/src/thread/semaphore.cc"
  "${NETTYCORE_ROOT}/src/thread/thread.cc")
target_include_directories(nettycore_thread_obj PUBLIC "${NETTYCORE_INCLUDE}")
target_compile_features(nettycore_thread_obj PUBLIC cxx_std_11)
target_link_libraries(nettycore_thread_obj PUBLIC nettycore_utils Threads::Threads)
add_library(nettycore_thread STATIC $<TARGET_OBJECTS:nettycore_thread_obj>)
add_library(nettycore::thread ALIAS nettycore_thread)

# ── memory ───────────────────────────────────────────────
add_library(nettycore_memory_obj OBJECT
  "${NETTYCORE_ROOT}/src/memory/stack_pool.cc")
target_include_directories(nettycore_memory_obj PUBLIC "${NETTYCORE_INCLUDE}")
target_compile_features(nettycore_memory_obj PUBLIC cxx_std_11)
target_link_libraries(nettycore_memory_obj PUBLIC Threads::Threads)
add_library(nettycore_memory STATIC $<TARGET_OBJECTS:nettycore_memory_obj>)
add_library(nettycore::memory ALIAS nettycore_memory)

# ── fiber ────────────────────────────────────────────────
add_library(nettycore_fiber_obj OBJECT
  "${NETTYCORE_ROOT}/src/fiber/fiber.cc"
  "${NETTYCORE_ROOT}/src/fiber/fiber_pool.cc"
  "${NETTYCORE_ROOT}/src/fiber/scheduler.cc"
  "${NETTYCORE_ROOT}/src/fiber/timer.cc"
  "${NETTYCORE_ROOT}/src/fiber/timing_wheel.cc"
  "${NETTYCORE_ROOT}/src/fiber/sync/fiber_mutex.cc"
  "${NETTYCORE_ROOT}/src/fiber/sync/fiber_semaphore.cc"
  "${NETTYCORE_ROOT}/src/fiber/sync/fiber_local.cc")
target_include_directories(nettycore_fiber_obj PUBLIC "${NETTYCORE_INCLUDE}")
target_compile_features(nettycore_fiber_obj PUBLIC cxx_std_11)
target_link_libraries(nettycore_fiber_obj PUBLIC
  nettycore_memory nettycore_utils nettycore_thread Threads::Threads)
add_library(nettycore_fiber STATIC $<TARGET_OBJECTS:nettycore_fiber_obj>)
target_link_libraries(nettycore_fiber PUBLIC
  nettycore_memory nettycore_utils nettycore_thread Threads::Threads)
add_library(nettycore::fiber ALIAS nettycore_fiber)

# ── io ───────────────────────────────────────────────────
add_library(nettycore_io_obj OBJECT
  "${NETTYCORE_ROOT}/src/io/fd_context.cc"
  "${NETTYCORE_ROOT}/src/io/reactor.cc"
  "${NETTYCORE_ROOT}/src/io/iomanager.cc"
  "${NETTYCORE_ROOT}/src/io/runtime.cc"
  "${NETTYCORE_ROOT}/src/io/hook.cc"
  "${NETTYCORE_ROOT}/src/io/connect.cc")
target_include_directories(nettycore_io_obj PUBLIC "${NETTYCORE_INCLUDE}")
target_compile_features(nettycore_io_obj PUBLIC cxx_std_11)
target_link_libraries(nettycore_io_obj PUBLIC
  nettycore_fiber nettycore_thread nettycore_utils Threads::Threads ${CMAKE_DL_LIBS})
add_library(nettycore_io STATIC $<TARGET_OBJECTS:nettycore_io_obj>)
add_library(nettycore::io ALIAS nettycore_io)

# ── buffer ───────────────────────────────────────────────
add_library(nettycore_buffer_obj OBJECT
  "${NETTYCORE_ROOT}/src/buffer/ring_buffer.cc"
  "${NETTYCORE_ROOT}/src/buffer/chain_buffer.cc"
  "${NETTYCORE_ROOT}/src/buffer/byte_array.cc")
target_include_directories(nettycore_buffer_obj PUBLIC "${NETTYCORE_INCLUDE}")
add_library(nettycore_buffer STATIC $<TARGET_OBJECTS:nettycore_buffer_obj>)
add_library(nettycore::buffer ALIAS nettycore_buffer)

# ── socket ───────────────────────────────────────────────
add_library(nettycore_socket_obj OBJECT
  "${NETTYCORE_ROOT}/src/socket/address.cc"
  "${NETTYCORE_ROOT}/src/socket/socket.cc"
  "${NETTYCORE_ROOT}/src/socket/stream.cc")
target_include_directories(nettycore_socket_obj PUBLIC "${NETTYCORE_INCLUDE}")
target_link_libraries(nettycore_socket_obj PUBLIC
  nettycore_io nettycore_buffer nettycore_utils Threads::Threads ${CMAKE_DL_LIBS})
add_library(nettycore_socket STATIC $<TARGET_OBJECTS:nettycore_socket_obj>)
add_library(nettycore::socket ALIAS nettycore_socket)

# ── server（L4）──────────────────────────────────────────
add_library(nettycore_server_obj OBJECT
  "${NETTYCORE_ROOT}/src/server/worker_group.cc"
  "${NETTYCORE_ROOT}/src/server/acceptor.cc"
  "${NETTYCORE_ROOT}/src/server/tcp_server.cc")
target_include_directories(nettycore_server_obj PUBLIC "${NETTYCORE_INCLUDE}")
target_compile_features(nettycore_server_obj PUBLIC cxx_std_11)
target_link_libraries(nettycore_server_obj PUBLIC
  nettycore_io nettycore_socket nettycore_utils Threads::Threads)
add_library(nettycore_server STATIC $<TARGET_OBJECTS:nettycore_server_obj>)
add_library(nettycore::server ALIAS nettycore_server)

# ── log + config 伴生库（YAML + lemo config/log）──
if(NETTYCORE_BUILD_CONFIG)
  find_package(yaml-cpp REQUIRED)

  add_library(nettycore_log_obj OBJECT
    "${LEMO_SRC_ROOT}/src/log/level.cc"
    "${LEMO_SRC_ROOT}/src/log/event.cc"
    "${LEMO_SRC_ROOT}/src/log/pattern_layout.cc"
    "${LEMO_SRC_ROOT}/src/log/appender.cc"
    "${LEMO_SRC_ROOT}/src/log/console_appender.cc"
    "${LEMO_SRC_ROOT}/src/log/file_appender.cc"
    "${LEMO_SRC_ROOT}/src/log/log_paths.cc"
    "${LEMO_SRC_ROOT}/src/log/rolling_file_appender.cc"
    "${LEMO_SRC_ROOT}/src/log/async_appender.cc"
    "${LEMO_SRC_ROOT}/src/log/log_runtime.cc"
    "${LEMO_SRC_ROOT}/src/log/appender_registry.cc"
    "${LEMO_SRC_ROOT}/src/log/mdc.cc"
    "${LEMO_SRC_ROOT}/src/log/logger.cc"
    "${LEMO_SRC_ROOT}/src/log/logger_repository.cc")
  target_include_directories(nettycore_log_obj PUBLIC
    "${NETTYCORE_INCLUDE}" "${LEMO_SRC_INCLUDE}")
  target_compile_features(nettycore_log_obj PUBLIC cxx_std_11)
  target_link_libraries(nettycore_log_obj PUBLIC
    nettycore_fiber nettycore_thread nettycore_utils Threads::Threads)
  add_library(nettycore_log STATIC $<TARGET_OBJECTS:nettycore_log_obj>)
  target_include_directories(nettycore_log PUBLIC
    "${NETTYCORE_INCLUDE}" "${LEMO_SRC_INCLUDE}")
  target_link_libraries(nettycore_log PUBLIC
    nettycore_fiber nettycore_thread nettycore_utils Threads::Threads)
  add_library(nettycore::log ALIAS nettycore_log)

  add_library(nettycore_config_obj OBJECT
    "${LEMO_SRC_ROOT}/src/config/config_var.cc"
    "${LEMO_SRC_ROOT}/src/config/config_center.cc"
    "${LEMO_SRC_ROOT}/src/config/property_loader.cc"
    "${LEMO_SRC_ROOT}/src/config/log_config.cc"
    "${LEMO_SRC_ROOT}/src/config/config_printer.cc"
    "${NETTYCORE_ROOT}/src/config/yaml_loader.cc"
    "${NETTYCORE_ROOT}/src/config/netty_config.cc")
  target_include_directories(nettycore_config_obj PUBLIC
    "${NETTYCORE_INCLUDE}" "${LEMO_SRC_INCLUDE}")
  target_compile_features(nettycore_config_obj PUBLIC cxx_std_11)
  target_link_libraries(nettycore_config_obj PUBLIC
    nettycore_log nettycore_io nettycore_memory nettycore_utils yaml-cpp)
  add_library(nettycore_config STATIC $<TARGET_OBJECTS:nettycore_config_obj>)
  target_include_directories(nettycore_config PUBLIC
    "${NETTYCORE_INCLUDE}" "${LEMO_SRC_INCLUDE}")
  target_link_libraries(nettycore_config PUBLIC
    nettycore_log nettycore_io nettycore_memory nettycore_utils yaml-cpp)
  add_library(nettycore::config ALIAS nettycore_config)

  foreach(_obj nettycore_log_obj nettycore_config_obj)
    nettycore_apply_perf_flags(${_obj})
    set_target_properties(${_obj} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  endforeach()
endif()

foreach(_obj nettycore_utils_obj nettycore_thread_obj nettycore_memory_obj
             nettycore_fiber_obj nettycore_io_obj nettycore_buffer_obj
             nettycore_socket_obj nettycore_server_obj)
  nettycore_apply_perf_flags(${_obj})
  set_target_properties(${_obj} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endforeach()

set(NETTYCORE_ALL_OBJECTS
  $<TARGET_OBJECTS:nettycore_utils_obj>
  $<TARGET_OBJECTS:nettycore_thread_obj>
  $<TARGET_OBJECTS:nettycore_memory_obj>
  $<TARGET_OBJECTS:nettycore_fiber_obj>
  $<TARGET_OBJECTS:nettycore_io_obj>
  $<TARGET_OBJECTS:nettycore_buffer_obj>
  $<TARGET_OBJECTS:nettycore_socket_obj>
  $<TARGET_OBJECTS:nettycore_server_obj>)

# ── 聚合库（静态 + 动态）──────────────────────────────────
add_library(nettycore STATIC ${NETTYCORE_ALL_OBJECTS})
target_include_directories(nettycore PUBLIC
  $<BUILD_INTERFACE:${NETTYCORE_INCLUDE}>
  $<INSTALL_INTERFACE:include>)
target_link_libraries(nettycore PUBLIC
  nettycore_server nettycore_socket nettycore_io nettycore_fiber nettycore_memory
  nettycore_buffer nettycore_thread nettycore_utils
  Threads::Threads ${CMAKE_DL_LIBS})
add_library(nettycore::nettycore ALIAS nettycore)

if(NETTYCORE_BUILD_SHARED)
  add_library(lemo_nettycore SHARED ${NETTYCORE_ALL_OBJECTS})
  target_include_directories(lemo_nettycore PUBLIC
    $<BUILD_INTERFACE:${NETTYCORE_INCLUDE}>
    $<INSTALL_INTERFACE:include>)
  target_compile_definitions(lemo_nettycore PRIVATE LEMO_NETTYCORE_BUILD_SHARED)
  target_link_libraries(lemo_nettycore PUBLIC
    Threads::Threads ${CMAKE_DL_LIBS})
  file(MAKE_DIRECTORY "${KVSTORE_LIB_ROOT}")
  set_target_properties(lemo_nettycore PROPERTIES
    OUTPUT_NAME lemo_nettycore
    VERSION 0.4.0
    SOVERSION 0
    LIBRARY_OUTPUT_DIRECTORY "${KVSTORE_LIB_ROOT}"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${KVSTORE_LIB_ROOT}"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${KVSTORE_LIB_ROOT}"
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${KVSTORE_LIB_ROOT}"
    LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${KVSTORE_LIB_ROOT}"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)
  nettycore_apply_perf_flags(lemo_nettycore)
  add_library(lemo::nettycore ALIAS lemo_nettycore)

  install(TARGETS lemo_nettycore
    EXPORT lemo_nettycoreTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(DIRECTORY "${NETTYCORE_INCLUDE}/lemo"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
  install(EXPORT lemo_nettycoreTargets
    FILE lemo_nettycoreTargets.cmake
    NAMESPACE lemo::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LemoNettyCore)
endif()

set(nettycore_bin "${KVSTORE_BIN_ROOT}/LemoNettyCore")
file(MAKE_DIRECTORY "${nettycore_bin}")

# ── 性能基准 ─────────────────────────────────────────────
add_executable(bench_echo_server_nettycore
  "${CMAKE_SOURCE_DIR}/tests/lemo/socket/bench_echo_server.cc")
target_link_libraries(bench_echo_server_nettycore PRIVATE
  nettycore_socket nettycore_io nettycore_fiber nettycore_memory nettycore_buffer
  nettycore_thread nettycore_utils)
target_include_directories(bench_echo_server_nettycore PRIVATE "${NETTYCORE_INCLUDE}")
set_target_properties(bench_echo_server_nettycore PROPERTIES
  OUTPUT_NAME bench_echo_server
  RUNTIME_OUTPUT_DIRECTORY "${nettycore_bin}")
nettycore_apply_perf_flags(bench_echo_server_nettycore)

add_executable(bench_scheduler_nettycore
  "${CMAKE_SOURCE_DIR}/tests/lemo/fiber/bench_scheduler.cc")
target_link_libraries(bench_scheduler_nettycore PRIVATE
  nettycore_fiber nettycore_memory nettycore_thread nettycore_utils)
target_include_directories(bench_scheduler_nettycore PRIVATE "${NETTYCORE_INCLUDE}")
set_target_properties(bench_scheduler_nettycore PROPERTIES
  OUTPUT_NAME bench_scheduler
  RUNTIME_OUTPUT_DIRECTORY "${nettycore_bin}")
nettycore_apply_perf_flags(bench_scheduler_nettycore)

add_custom_target(bench_nettycore_perf
  COMMAND "${nettycore_bin}/bench_echo_server"
    --mode local --threads 4 --connections 64 --payload 128 --messages 1000
  COMMAND "${nettycore_bin}/bench_scheduler" --quick
  DEPENDS bench_echo_server_nettycore bench_scheduler_nettycore
  COMMENT "运行 LemoNettyCore 终版性能基准")

# ── 终版单元/集成测试 ────────────────────────────────────
set(NETTYCORE_TEST_LIBS_IO
  nettycore_io nettycore_fiber nettycore_thread nettycore_utils)
set(NETTYCORE_TEST_LIBS_FIBER
  nettycore_fiber nettycore_memory nettycore_io nettycore_thread nettycore_utils)
set(NETTYCORE_TEST_LIBS_MEMORY
  nettycore_memory nettycore_thread nettycore_utils)
set(NETTYCORE_TEST_LIBS_BUFFER
  nettycore_buffer nettycore_utils)

set(NETTYCORE_TEST_LIBS_SOCKET
  nettycore_server nettycore_socket nettycore_io nettycore_fiber nettycore_memory
  nettycore_buffer nettycore_thread nettycore_utils)
set(NETTYCORE_TEST_LIBS_SERVER
  nettycore_server nettycore_socket nettycore_io nettycore_fiber nettycore_thread
  nettycore_utils)

function(nettycore_add_test module_path test_name libs)
  set(test_bin "${KVSTORE_BIN_ROOT}/LemoNettyCore/${module_path}")
  file(MAKE_DIRECTORY "${test_bin}")
  set(target "${test_name}_nettycore")
  add_executable(${target}
    "${CMAKE_SOURCE_DIR}/tests/lemo/${module_path}/${test_name}.cc")
  target_link_libraries(${target} PRIVATE ${libs})
  target_include_directories(${target} PRIVATE "${NETTYCORE_INCLUDE}")
  if(module_path STREQUAL "fiber")
    target_include_directories(${target} PRIVATE
      "${CMAKE_SOURCE_DIR}/tests/lemo/fiber"
      "${CMAKE_SOURCE_DIR}/tests/lemo/io")
  elseif(module_path STREQUAL "socket" OR module_path STREQUAL "buffer")
    target_include_directories(${target} PRIVATE
      "${CMAKE_SOURCE_DIR}/tests/lemo/io")
  endif()
  set_target_properties(${target} PROPERTIES
    OUTPUT_NAME ${test_name}
    RUNTIME_OUTPUT_DIRECTORY "${test_bin}")
  nettycore_apply_perf_flags(${target})
  string(REPLACE "/" "." ctest_module "LemoNettyCore.${module_path}")
  add_test(NAME "${ctest_module}.${test_name}"
    COMMAND "${test_bin}/${test_name}")
  list(APPEND KVSTORE_ALL_TEST_TARGETS ${target})
  set(KVSTORE_ALL_TEST_TARGETS "${KVSTORE_ALL_TEST_TARGETS}" CACHE INTERNAL "" FORCE)
endfunction()

nettycore_add_test(io test_iomanager "${NETTYCORE_TEST_LIBS_IO}")
nettycore_add_test(io test_hook "${NETTYCORE_TEST_LIBS_IO}")
nettycore_add_test(fiber test_fiber "${NETTYCORE_TEST_LIBS_FIBER}")
nettycore_add_test(fiber test_scheduler "${NETTYCORE_TEST_LIBS_FIBER}")
nettycore_add_test(fiber test_timer "${NETTYCORE_TEST_LIBS_FIBER}")
nettycore_add_test(fiber test_timer_wheel "${NETTYCORE_TEST_LIBS_FIBER}")
nettycore_add_test(fiber test_stack_pool "${NETTYCORE_TEST_LIBS_MEMORY}")
nettycore_add_test(fiber test_fiber_sleep_iomanager "${NETTYCORE_TEST_LIBS_FIBER}")
nettycore_add_test(fiber test_fiber_mutex "${NETTYCORE_TEST_LIBS_FIBER}")
nettycore_add_test(fiber test_fiber_semaphore "${NETTYCORE_TEST_LIBS_FIBER}")
nettycore_add_test(fiber test_fiber_local "${NETTYCORE_TEST_LIBS_FIBER}")
nettycore_add_test(buffer test_ring_buffer "${NETTYCORE_TEST_LIBS_BUFFER}")
nettycore_add_test(buffer test_chain_buffer "${NETTYCORE_TEST_LIBS_BUFFER}")
nettycore_add_test(buffer test_byte_array "${NETTYCORE_TEST_LIBS_BUFFER}")
nettycore_add_test(socket test_address "${NETTYCORE_TEST_LIBS_SOCKET}")
nettycore_add_test(socket test_echo_server "${NETTYCORE_TEST_LIBS_SOCKET}")

function(nettycore_add_test_in module_path test_name libs)
  set(test_bin "${KVSTORE_BIN_ROOT}/LemoNettyCore/${module_path}")
  file(MAKE_DIRECTORY "${test_bin}")
  set(target "${test_name}_nettycore")
  add_executable(${target}
    "${CMAKE_SOURCE_DIR}/tests/lemo/${module_path}/${test_name}.cc")
  target_link_libraries(${target} PRIVATE ${libs})
  target_include_directories(${target} PRIVATE "${NETTYCORE_INCLUDE}"
    "${CMAKE_SOURCE_DIR}/tests/lemo/io")
  set_target_properties(${target} PROPERTIES
    OUTPUT_NAME ${test_name}
    RUNTIME_OUTPUT_DIRECTORY "${test_bin}")
  nettycore_apply_perf_flags(${target})
  add_test(NAME "LemoNettyCore.${module_path}.${test_name}"
    COMMAND "${test_bin}/${test_name}")
  list(APPEND KVSTORE_ALL_TEST_TARGETS ${target})
  set(KVSTORE_ALL_TEST_TARGETS "${KVSTORE_ALL_TEST_TARGETS}" CACHE INTERNAL "" FORCE)
endfunction()

nettycore_add_test_in(server test_tcp_server "${NETTYCORE_TEST_LIBS_SERVER}")

add_custom_target(run_tests_nettycore
  COMMAND ${CMAKE_CTEST_COMMAND} -R "^LemoNettyCore\\." --output-on-failure
  DEPENDS
    test_iomanager_nettycore test_hook_nettycore
    test_fiber_nettycore test_scheduler_nettycore test_timer_nettycore
    test_timer_wheel_nettycore test_stack_pool_nettycore
    test_fiber_sleep_iomanager_nettycore test_fiber_mutex_nettycore
    test_fiber_semaphore_nettycore test_fiber_local_nettycore
    test_ring_buffer_nettycore test_chain_buffer_nettycore test_byte_array_nettycore
    test_address_nettycore test_echo_server_nettycore test_tcp_server_nettycore
  COMMENT "运行 LemoNettyCore 终版全套测试")

add_custom_target(bench_echo_server_nettycore_quick
  COMMAND "${nettycore_bin}/bench_echo_server" --quick
  DEPENDS bench_echo_server_nettycore
  COMMENT "LemoNettyCore echo 快速 smoke")

if(NETTYCORE_BUILD_CONFIG)
  set(nettycore_config_bin "${KVSTORE_BIN_ROOT}/LemoNettyCore/config")
  file(MAKE_DIRECTORY "${nettycore_config_bin}")

  add_executable(test_netty_config_nettycore
    "${CMAKE_SOURCE_DIR}/tests/lemo/config/test_netty_config.cc")
  target_link_libraries(test_netty_config_nettycore PRIVATE
    nettycore_config nettycore_log nettycore_server nettycore_socket nettycore_io
    nettycore_fiber nettycore_memory nettycore_thread nettycore_utils)
  target_include_directories(test_netty_config_nettycore PRIVATE
    "${NETTYCORE_INCLUDE}" "${LEMO_SRC_INCLUDE}"
    "${CMAKE_SOURCE_DIR}/tests/lemo/config")
  target_compile_definitions(test_netty_config_nettycore PRIVATE
    NETTYCORE_TEST_CONF="${CMAKE_SOURCE_DIR}/tests/lemo/config/fixtures/nettycore_test.yaml")
  set_target_properties(test_netty_config_nettycore PROPERTIES
    OUTPUT_NAME test_netty_config
    RUNTIME_OUTPUT_DIRECTORY "${nettycore_config_bin}")
  nettycore_apply_perf_flags(test_netty_config_nettycore)
  add_test(NAME "LemoNettyCore.config.test_netty_config"
    COMMAND "${nettycore_config_bin}/test_netty_config")
  list(APPEND KVSTORE_ALL_TEST_TARGETS test_netty_config_nettycore)
  set(KVSTORE_ALL_TEST_TARGETS "${KVSTORE_ALL_TEST_TARGETS}" CACHE INTERNAL "" FORCE)

  add_executable(nettycore_config_demo
    "${CMAKE_SOURCE_DIR}/tests/lemo/config/nettycore_config_demo.cc")
  target_link_libraries(nettycore_config_demo PRIVATE
    nettycore_config nettycore_log nettycore_server nettycore_socket nettycore_io
    nettycore_fiber nettycore_memory nettycore_thread nettycore_utils)
  target_include_directories(nettycore_config_demo PRIVATE
    "${NETTYCORE_INCLUDE}" "${LEMO_SRC_INCLUDE}")
  target_compile_definitions(nettycore_config_demo PRIVATE
    NETTYCORE_DEFAULT_CONF="${CMAKE_SOURCE_DIR}/tests/lemo/config/fixtures/nettycore_test.yaml")
  set_target_properties(nettycore_config_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${nettycore_config_bin}")
  nettycore_apply_perf_flags(nettycore_config_demo)

  add_custom_target(nettycore_config_demo_run
    COMMAND "${nettycore_config_bin}/nettycore_config_demo"
    DEPENDS nettycore_config_demo
    COMMENT "运行 LemoNettyCore 配置完整示例")

  add_custom_target(run_tests_nettycore_config
    COMMAND ${CMAKE_CTEST_COMMAND} -R "^LemoNettyCore\\.config\\." --output-on-failure
    DEPENDS test_netty_config_nettycore
    COMMENT "运行 LemoNettyCore config 模块测试")
endif()
