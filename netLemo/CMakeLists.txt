cmake_minimum_required(VERSION 3.10)

set(NETLEMO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(NETLEMO_INCLUDE "${NETLEMO_ROOT}/include")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Threads REQUIRED)

function(netlemo_apply_perf_flags target_name)
  if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_options(${target_name} PRIVATE -O3 -DNDEBUG)
  endif()
endfunction()

# ── infra：utils + thread（netCore 复用）──────────────────
add_library(netlemo_utils_obj OBJECT
  "${NETLEMO_ROOT}/src/utils/string_util.cc"
  "${NETLEMO_ROOT}/src/utils/thread_util.cc"
  "${NETLEMO_ROOT}/src/utils/time_util.cc")
target_include_directories(netlemo_utils_obj PUBLIC "${NETLEMO_INCLUDE}")
target_compile_features(netlemo_utils_obj PUBLIC cxx_std_11)
add_library(netlemo_utils STATIC $<TARGET_OBJECTS:netlemo_utils_obj>)
target_include_directories(netlemo_utils PUBLIC "${NETLEMO_INCLUDE}")
add_library(netlemo::utils ALIAS netlemo_utils)

add_library(netlemo_thread_obj OBJECT
  "${NETLEMO_ROOT}/src/thread/semaphore.cc"
  "${NETLEMO_ROOT}/src/thread/thread.cc")
target_include_directories(netlemo_thread_obj PUBLIC "${NETLEMO_INCLUDE}")
target_compile_features(netlemo_thread_obj PUBLIC cxx_std_11)
target_link_libraries(netlemo_thread_obj PUBLIC netlemo_utils Threads::Threads)
add_library(netlemo_thread STATIC $<TARGET_OBJECTS:netlemo_thread_obj>)
add_library(netlemo::thread ALIAS netlemo_thread)

foreach(_obj netlemo_utils_obj netlemo_thread_obj)
  netlemo_apply_perf_flags(${_obj})
endforeach()

# ── 完整 netLemo 栈（NETCORE_BUILD 时由 netCore 替代 runtime 层）──
if(NOT NETCORE_BUILD)
  add_library(netlemo_memory_obj OBJECT "${NETLEMO_ROOT}/src/memory/stack_pool.cc")
  target_include_directories(netlemo_memory_obj PUBLIC "${NETLEMO_INCLUDE}")
  target_compile_features(netlemo_memory_obj PUBLIC cxx_std_11)
  target_link_libraries(netlemo_memory_obj PUBLIC Threads::Threads)
  add_library(netlemo_memory STATIC $<TARGET_OBJECTS:netlemo_memory_obj>)
  target_include_directories(netlemo_memory PUBLIC "${NETLEMO_INCLUDE}")
  add_library(netlemo::memory ALIAS netlemo_memory)

  add_library(netlemo_fiber_obj OBJECT
    "${NETLEMO_ROOT}/src/fiber/fiber.cc"
    "${NETLEMO_ROOT}/src/fiber/fiber_pool.cc"
    "${NETLEMO_ROOT}/src/fiber/scheduler.cc"
    "${NETLEMO_ROOT}/src/fiber/timer.cc"
    "${NETLEMO_ROOT}/src/fiber/timing_wheel.cc")
  target_include_directories(netlemo_fiber_obj PUBLIC "${NETLEMO_INCLUDE}")
  target_compile_features(netlemo_fiber_obj PUBLIC cxx_std_11)
  target_link_libraries(netlemo_fiber_obj PUBLIC netlemo_memory netlemo_utils netlemo_thread Threads::Threads)
  add_library(netlemo_fiber STATIC $<TARGET_OBJECTS:netlemo_fiber_obj>)
  add_library(netlemo::fiber ALIAS netlemo_fiber)

  add_library(netlemo_io_obj OBJECT
    "${NETLEMO_ROOT}/src/io/fd_context.cc"
    "${NETLEMO_ROOT}/src/io/reactor.cc"
    "${NETLEMO_ROOT}/src/io/iomanager.cc"
    "${NETLEMO_ROOT}/src/io/hook.cc"
    "${NETLEMO_ROOT}/src/io/connect.cc"
    "${NETLEMO_ROOT}/src/io/runtime.cc")
  target_include_directories(netlemo_io_obj PUBLIC "${NETLEMO_INCLUDE}")
  target_compile_features(netlemo_io_obj PUBLIC cxx_std_11)
  target_link_libraries(netlemo_io_obj PUBLIC netlemo_fiber netlemo_thread netlemo_utils Threads::Threads ${CMAKE_DL_LIBS})
  add_library(netlemo_io STATIC $<TARGET_OBJECTS:netlemo_io_obj>)
  add_library(netlemo::io ALIAS netlemo_io)

  add_library(netlemo_buffer_obj OBJECT "${NETLEMO_ROOT}/src/buffer/ring_buffer.cc")
  target_include_directories(netlemo_buffer_obj PUBLIC "${NETLEMO_INCLUDE}")
  add_library(netlemo_buffer STATIC $<TARGET_OBJECTS:netlemo_buffer_obj>)
  add_library(netlemo::buffer ALIAS netlemo_buffer)

  add_library(netlemo_socket_obj OBJECT
    "${NETLEMO_ROOT}/src/socket/address.cc"
    "${NETLEMO_ROOT}/src/socket/socket.cc")
  target_include_directories(netlemo_socket_obj PUBLIC "${NETLEMO_INCLUDE}")
  target_link_libraries(netlemo_socket_obj PUBLIC netlemo_io netlemo_buffer netlemo_utils Threads::Threads ${CMAKE_DL_LIBS})
  add_library(netlemo_socket STATIC $<TARGET_OBJECTS:netlemo_socket_obj>)
  add_library(netlemo::socket ALIAS netlemo_socket)

  foreach(_obj netlemo_memory_obj netlemo_fiber_obj netlemo_io_obj netlemo_buffer_obj netlemo_socket_obj)
    netlemo_apply_perf_flags(${_obj})
  endforeach()

  set(netlemo_bin "${KVSTORE_BIN_ROOT}/netLemo")
  file(MAKE_DIRECTORY "${netlemo_bin}")

  add_executable(bench_echo_server_netlemo
    "${CMAKE_SOURCE_DIR}/tests/lemo/socket/bench_echo_server.cc")
  target_link_libraries(bench_echo_server_netlemo PRIVATE
    netlemo_socket netlemo_io netlemo_fiber netlemo_memory netlemo_buffer netlemo_thread netlemo_utils)
  target_include_directories(bench_echo_server_netlemo PRIVATE "${NETLEMO_INCLUDE}")
  set_target_properties(bench_echo_server_netlemo PROPERTIES
    OUTPUT_NAME bench_echo_server
    RUNTIME_OUTPUT_DIRECTORY "${netlemo_bin}")
  netlemo_apply_perf_flags(bench_echo_server_netlemo)

  add_custom_target(bench_echo_server_netlemo_run
    COMMAND "${netlemo_bin}/bench_echo_server" --quick
    DEPENDS bench_echo_server_netlemo
    COMMENT "运行 netLemo echo 性能基准")
endif()
