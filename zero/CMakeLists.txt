cmake_minimum_required(VERSION 3.14)
project(zero VERSION 1.0.0 LANGUAGES CXX ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 汇编支持 (context.S)
enable_language(ASM)

# 编译选项
set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -g3 -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -mtune=native")

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported)
    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    endif()
endif()

# ============================================================
# zero 核心库 (按 Phase 增量添加)
# ============================================================
set(ZERO_SOURCES
    # Phase 1: fiber
    fiber/context.S
    fiber/fiber.cc
    fiber/stack_pool.cc
    fiber/fiber_pool.cc

    # Phase 1: base util
    util/util.cc

    # Phase 2: scheduler
    scheduler/scheduler.cc
    scheduler/work_stealing_queue.cc
    scheduler/timer_wheel.cc
    scheduler/reactor.cc
    scheduler/fd_manager.cc
    scheduler/hook.cc

    # Phase 4: net
    net/buffer.cc
    net/address.cc
    net/socket.cc
    net/stream.cc
    net/socket_stream.cc

    # Phase 5: log
    log/log.cc

    # Phase 6: tcp_server
    net/tcp_server.cc
)

add_library(zero STATIC ${ZERO_SOURCES})

target_include_directories(zero PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(zero PUBLIC
    pthread
    dl
)

# 示例程序
option(ZERO_BUILD_EXAMPLES "Build examples" ON)
if(ZERO_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
