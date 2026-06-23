cmake_minimum_required(VERSION 3.14)
project(zero VERSION 2.0.0 LANGUAGES CXX ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================
# zero library — high-performance network framework
# ============================================================
set(ZERO_SOURCES
    src/zero_init.cc

    # Config
    src/config/config.cc

    # Fiber
    src/fiber/fiber.cc
    src/fiber/fiber_pool.cc
    src/fiber/stack_pool.cc
    src/fiber/channel.cc
    src/fiber/context.S

    # Scheduler
    src/scheduler/scheduler.cc
    src/scheduler/reactor.cc
    src/scheduler/timer_wheel.cc
    src/scheduler/fd_manager.cc
    src/scheduler/work_stealing_queue.cc
    src/scheduler/hook.cc

    # Network
    src/net/address.cc
    src/net/buffer.cc
    src/net/socket.cc
    src/net/socket_stream.cc
    src/net/stream.cc
    src/net/tcp_client.cc
    src/net/tcp_server.cc

    # I/O
    src/io/epoll_engine.cc
    src/io/uring_engine.cc

    # Log
    src/log/log.cc
)

add_library(zero ${ZERO_SOURCES})

target_include_directories(zero PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Compiler options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(zero PRIVATE
        -Wall -Wextra -Wpedantic
        -march=native -mtune=native
    )
endif()

# Enable ASM for context switching
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    enable_language(ASM)
    set_source_files_properties(
        src/fiber/context.S
        PROPERTIES COMPILE_FLAGS "-x assembler-with-cpp"
    )
endif()

# Dependencies
find_package(Threads REQUIRED)
target_link_libraries(zero PUBLIC Threads::Threads)

# Optional: io_uring
find_package(LibUring QUIET)
if(LibUring_FOUND)
    target_link_libraries(zero PRIVATE LibUring::LibUring)
    target_compile_definitions(zero PUBLIC ZERO_HAS_IOURING=1)
endif()

# ============================================================
# Tests
# ============================================================
option(ZERO_BUILD_TESTS "Build zero tests" ON)

if(ZERO_BUILD_TESTS)
    enable_testing()

    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG        v1.14.0
        )
        FetchContent_MakeAvailable(googletest)
    endif()

    add_subdirectory(tests)
endif()

# Install
include(GNUInstallDirs)
install(DIRECTORY include/zero DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS zero DESTINATION ${CMAKE_INSTALL_LIBDIR})
