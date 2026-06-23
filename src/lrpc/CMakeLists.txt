cmake_minimum_required(VERSION 3.14)
project(lrpc VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# lrpc is header-only
add_library(lrpc INTERFACE)

target_include_directories(lrpc INTERFACE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(lrpc INTERFACE pthread)
