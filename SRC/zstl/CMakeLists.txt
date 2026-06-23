cmake_minimum_required(VERSION 3.14)
project(zstl VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================
# zstl is primarily header-only with a small compiled library
# for the memory pool slow paths and string internals
# ============================================================
add_library(zstl
    src/pool.cc
    src/string.cc
)

target_include_directories(zstl PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Compiler options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(zstl PRIVATE
        -Wall -Wextra -Wpedantic -Werror
        -march=native -mtune=native
    )
endif()

# Link with pthread for concurrency support
find_package(Threads REQUIRED)
target_link_libraries(zstl PUBLIC Threads::Threads)

# ============================================================
# Tests
# ============================================================
option(ZSTL_BUILD_TESTS "Build zstl tests" ON)

if(ZSTL_BUILD_TESTS)
    enable_testing()

    # Find Google Test
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

# ============================================================
# Benchmarks
# ============================================================
option(ZSTL_BUILD_BENCH "Build zstl benchmarks" OFF)

# Install
include(GNUInstallDirs)
install(DIRECTORY include/zstl DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS zstl DESTINATION ${CMAKE_INSTALL_LIBDIR})
