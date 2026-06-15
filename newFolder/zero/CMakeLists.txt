cmake_minimum_required(VERSION 3.10)
project(zero VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Link to lstl (header-only, sibling directory)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../lstl ${CMAKE_BINARY_DIR}/lstl_build)

# Zero library
add_library(zero STATIC
    src/context.cc
    src/fiber.cc
    src/reactor.cc
)

target_include_directories(zero PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(zero PUBLIC lstl)

# Tests
option(ZERO_BUILD_TESTS "Build zero tests" ON)
if(ZERO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
