cmake_minimum_required(VERSION 3.14)
project(ledis VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -g3 -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -mtune=native")

# ============================================================
# ledis 核心库 (header-only + command.cc)
# ============================================================
set(LEDIS_SOURCES
    core/command.cc
)

add_library(ledis STATIC ${LEDIS_SOURCES})

# Include 路径: KVStore 根 (用于 #include "kv_ledis/..." 和 #include "zero/...")
target_include_directories(ledis PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../newFolder/lstl/include
)

# 链接 zero 库
set(ZERO_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../zero/build")
if(EXISTS "${ZERO_BUILD_DIR}")
    find_library(ZERO_LIB zero PATHS "${ZERO_BUILD_DIR}" NO_DEFAULT_PATH)
    if(ZERO_LIB)
        message(STATUS "Found zero library: ${ZERO_LIB}")
        target_link_libraries(ledis PUBLIC ${ZERO_LIB})
    else()
        message(WARNING "zero library not found, adding subdirectory")
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../zero ${CMAKE_BINARY_DIR}/zero)
        target_link_libraries(ledis PUBLIC zero)
    endif()
else()
    message(WARNING "zero build dir not found, adding subdirectory")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../zero ${CMAKE_BINARY_DIR}/zero)
    target_link_libraries(ledis PUBLIC zero)
endif()

# luajit
find_library(LUAJIT_LIB luajit-5.1 PATHS /usr/lib/aarch64-linux-gnu)
find_path(LUAJIT_INCLUDE luajit.h PATHS /usr/include/luajit-2.1 /usr/include)
if(LUAJIT_LIB AND LUAJIT_INCLUDE)
    message(STATUS "Found luajit: ${LUAJIT_LIB}, ${LUAJIT_INCLUDE}")
    target_include_directories(ledis PUBLIC ${LUAJIT_INCLUDE})
else()
    message(WARNING "luajit not found, Lua disabled")
endif()

target_link_libraries(ledis PUBLIC pthread dl)

# jemalloc 可选 (单线程下 glibc 已足够快, 多线程时可启用)
# if(EXISTS "/usr/lib/aarch64-linux-gnu/libjemalloc.so.2")
#     target_link_libraries(ledis PUBLIC /usr/lib/aarch64-linux-gnu/libjemalloc.so.2)
# endif()

# io_uring
find_library(URING_LIB uring PATHS /usr/lib/aarch64-linux-gnu /usr/lib)
if(URING_LIB)
    message(STATUS "Found liburing: ${URING_LIB}")
else()
    message(STATUS "liburing not found, io_uring disabled")
endif()

# ============================================================
# ledis-server 可执行文件
# ============================================================
add_executable(ledis-server main.cc)
target_link_libraries(ledis-server ledis)
if(URING_LIB)
    target_link_libraries(ledis-server ${URING_LIB})
endif()
if(LUAJIT_LIB)
    target_link_libraries(ledis-server ${LUAJIT_LIB})
endif()
