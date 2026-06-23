cmake_minimum_required(VERSION 3.14)
project(KVStore VERSION 2.0.0 LANGUAGES CXX ASM
    DESCRIPTION "高性能 Redis 兼容 KV 存储系统 — 含自研网络库、容器库、RPC 框架"
)

# ============================================================
# Global C++ Standard
# ============================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================
# Output directories (per build-tree, not global)
# Debug/Release 分别放在 build/debug/ 和 build/release/ 下
# ============================================================

# ============================================================
# Build Options
# ============================================================
option(BUILD_TESTS "Build all unit tests (requires gtest)" ON)
option(BUILD_BENCHMARKS "Build performance benchmarks" OFF)
option(BUILD_STRESS_TESTS "Build stress/concurrency tests" OFF)
option(BUILD_EXAMPLES "Build example programs" ON)

# ============================================================
# Code coverage (optional)
# ============================================================
include(cmake/coverage.cmake)

# ============================================================
# Compiler-specific settings
# ============================================================
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -g3 -fno-omit-frame-pointer")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -mtune=native")

    # Link-Time Optimization for release
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        include(CheckIPOSupported)
        check_ipo_supported(RESULT ipo_supported)
        if(ipo_supported)
            set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        endif()
    endif()
endif()

# ============================================================
# Fetch gtest if building tests
# ============================================================
if(BUILD_TESTS OR BUILD_BENCHMARKS OR BUILD_STRESS_TESTS)
    include(cmake/gtest.cmake)
    enable_testing()
endif()

# ============================================================
# External dependencies
# ============================================================
find_package(yaml-cpp REQUIRED)
find_package(Threads REQUIRED)

# Optional: luajit for Lua scripting in ledis
find_library(LUAJIT_LIB luajit-5.1 PATHS /usr/lib/aarch64-linux-gnu)
find_path(LUAJIT_INCLUDE luajit.h PATHS /usr/include/luajit-2.1 /usr/include)

# Optional: io_uring for experimental server backend
find_library(URING_LIB uring PATHS /usr/lib/aarch64-linux-gnu /usr/lib)

# ============================================================
# Subdirectories (build libraries from src/)
# ============================================================

# Disable subproject test/benchmark builds — we manage them from tests/
set(LSTL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LSTL_BUILD_BENCH OFF CACHE BOOL "" FORCE)
set(ZERO_BUILD_EXAMPLES ${BUILD_EXAMPLES} CACHE BOOL "" FORCE)

# lstl: header-only, no compilation needed
add_subdirectory(src/lstl)

# zero: must be built before ledis and lrpc (depends on it)
add_subdirectory(src/zero)

# lrpc: header-only, depends on zero headers
add_subdirectory(src/lrpc)

# ledis: depends on zero, lstl
# Pass hints so ledis can find already-built targets
set(ZERO_LIBRARY_BUILT TRUE CACHE BOOL "" FORCE)
add_subdirectory(src/ledis)

# ============================================================
# Stress / Benchmark tests
# ============================================================
if(BUILD_STRESS_TESTS)
    add_subdirectory(tests/stress)
endif()

# ============================================================
# Tests (if enabled)
# ============================================================
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
