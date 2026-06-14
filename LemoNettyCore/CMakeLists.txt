cmake_minimum_required(VERSION 3.10)

set(NETTYCORE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(NETTYCORE_INCLUDE "${NETTYCORE_ROOT}/include")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Threads REQUIRED)

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
  "${NETTYCORE_ROOT}/src/fiber/timing_wheel.cc")
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
  "${NETTYCORE_ROOT}/src/buffer/ring_buffer.cc")
target_include_directories(nettycore_buffer_obj PUBLIC "${NETTYCORE_INCLUDE}")
add_library(nettycore_buffer STATIC $<TARGET_OBJECTS:nettycore_buffer_obj>)
add_library(nettycore::buffer ALIAS nettycore_buffer)

# ── socket ───────────────────────────────────────────────
add_library(nettycore_socket_obj OBJECT
  "${NETTYCORE_ROOT}/src/socket/address.cc"
  "${NETTYCORE_ROOT}/src/socket/socket.cc")
target_include_directories(nettycore_socket_obj PUBLIC "${NETTYCORE_INCLUDE}")
target_link_libraries(nettycore_socket_obj PUBLIC
  nettycore_io nettycore_buffer nettycore_utils Threads::Threads ${CMAKE_DL_LIBS})
add_library(nettycore_socket STATIC $<TARGET_OBJECTS:nettycore_socket_obj>)
add_library(nettycore::socket ALIAS nettycore_socket)

foreach(_obj nettycore_utils_obj nettycore_thread_obj nettycore_memory_obj
             nettycore_fiber_obj nettycore_io_obj nettycore_buffer_obj
             nettycore_socket_obj)
  nettycore_apply_perf_flags(${_obj})
endforeach()

# ── 聚合库（核心网络半栈）────────────────────────────────
add_library(nettycore STATIC
  $<TARGET_OBJECTS:nettycore_utils_obj>
  $<TARGET_OBJECTS:nettycore_thread_obj>
  $<TARGET_OBJECTS:nettycore_memory_obj>
  $<TARGET_OBJECTS:nettycore_fiber_obj>
  $<TARGET_OBJECTS:nettycore_io_obj>
  $<TARGET_OBJECTS:nettycore_buffer_obj>
  $<TARGET_OBJECTS:nettycore_socket_obj>)
target_include_directories(nettycore PUBLIC "${NETTYCORE_INCLUDE}")
target_link_libraries(nettycore PUBLIC
  nettycore_socket nettycore_io nettycore_fiber nettycore_memory
  nettycore_buffer nettycore_thread nettycore_utils
  Threads::Threads ${CMAKE_DL_LIBS})
add_library(nettycore::nettycore ALIAS nettycore)

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
  nettycore_socket nettycore_io nettycore_fiber nettycore_memory
  nettycore_buffer nettycore_thread nettycore_utils)

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
      "${CMAKE_SOURCE_DIR}/tests/lemo/fiber")
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
nettycore_add_test(buffer test_ring_buffer "${NETTYCORE_TEST_LIBS_BUFFER}")
nettycore_add_test(socket test_address "${NETTYCORE_TEST_LIBS_SOCKET}")
nettycore_add_test(socket test_echo_server "${NETTYCORE_TEST_LIBS_SOCKET}")

add_custom_target(run_tests_nettycore
  COMMAND ${CMAKE_CTEST_COMMAND} -R "^LemoNettyCore\\." --output-on-failure
  DEPENDS
    test_iomanager_nettycore test_hook_nettycore
    test_fiber_nettycore test_scheduler_nettycore test_timer_nettycore
    test_timer_wheel_nettycore test_stack_pool_nettycore
    test_fiber_sleep_iomanager_nettycore
    test_ring_buffer_nettycore test_address_nettycore test_echo_server_nettycore
  COMMENT "运行 LemoNettyCore 终版全套测试")

add_custom_target(bench_echo_server_nettycore_quick
  COMMAND "${nettycore_bin}/bench_echo_server" --quick
  DEPENDS bench_echo_server_nettycore
  COMMENT "LemoNettyCore echo 快速 smoke")
