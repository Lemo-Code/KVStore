cmake_minimum_required(VERSION 3.14)
project(ledis VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -g3 -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -mtune=native")

# ============================================================
# ledis 核心库
# ============================================================
set(LEDIS_SOURCES
    # Command dispatch (Phase 1)
    cmd/cmd_table.cc
)

add_library(ledis STATIC ${LEDIS_SOURCES})

# Include 路径
#   ledis 源码根: 用于 #include "ledis/..."
#   KVStore 根:   用于 #include "zero/..." (zero 库的 include 根)
target_include_directories(ledis PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../newFolder/lstl/include
)

# 链接 zero 库 (从 zero 构建目录)
# 假设 zero 已在 ../zero/build 构建
set(ZERO_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../zero/build")
if(EXISTS "${ZERO_BUILD_DIR}")
    find_library(ZERO_LIB zero PATHS "${ZERO_BUILD_DIR}" NO_DEFAULT_PATH)
    if(ZERO_LIB)
        message(STATUS "Found zero library: ${ZERO_LIB}")
        target_link_libraries(ledis PUBLIC ${ZERO_LIB})
    else()
        message(WARNING "zero library not found in ${ZERO_BUILD_DIR}")
        message(STATUS "You may need to build zero first: cd ../zero/build && cmake .. && make -j")
    endif()
else()
    message(WARNING "zero build directory not found: ${ZERO_BUILD_DIR}")
    message(STATUS "Using add_subdirectory to build zero...")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../zero ${CMAKE_BINARY_DIR}/zero)
    target_link_libraries(ledis PUBLIC zero)
endif()

target_link_libraries(ledis PUBLIC
    pthread
    dl
)

# ============================================================
# ledis-server 可执行文件
# ============================================================
add_executable(ledis-server examples/ledis_main.cc)
target_link_libraries(ledis-server ledis)
