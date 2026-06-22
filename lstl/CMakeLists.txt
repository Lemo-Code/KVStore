cmake_minimum_required(VERSION 3.10)
project(lstl VERSION 1.0.0 LANGUAGES CXX)

# C++14 standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# lstl is header-only
add_library(lstl INTERFACE)
target_include_directories(lstl INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Compiler options for strictness
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(lstl INTERFACE
        -Wall -Wextra -Wpedantic
    )
endif()

# Optional: build tests
option(LSTL_BUILD_TESTS "Build lstl tests" ON)

if(LSTL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Optional: build benchmarks
option(LSTL_BUILD_BENCH "Build lstl benchmarks" ON)

if(LSTL_BUILD_BENCH)
    # Threads library for concurrent benchmarks
    find_package(Threads REQUIRED)
    add_subdirectory(bench)
endif()

# Install
include(GNUInstallDirs)
install(DIRECTORY include/lstl DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
