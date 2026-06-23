# stress/ — 并发矩阵压测
#
# 输出目录由 shell 脚本传入; 二进制写入 .log 规范格式

set(STRESS_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/common)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

function(stress_target name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
    if(NOT ARG_SOURCES)
        set(ARG_SOURCES ${name}.cpp)
    endif()
    add_executable(${name} ${ARG_SOURCES})
    target_include_directories(${name} PRIVATE
        ${STRESS_COMMON_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/lstl/include
    )
    target_link_libraries(${name} PRIVATE ${ARG_LIBS} pthread)
    target_compile_options(${name} PRIVATE -O2 -DNDEBUG)
endfunction()

stress_target(stress_lstl_bench
    SOURCES lstl/stress_lstl_bench.cpp
    LIBS lstl
)

stress_target(stress_ledis_engine
    SOURCES ledis/stress_ledis_engine.cpp
    LIBS ledis
)

stress_target(stress_ledis_net
    SOURCES ledis/stress_ledis_net.cpp
    LIBS
)

stress_target(stress_ledis_redis_compare
    SOURCES ledis/stress_ledis_redis_compare.cpp
    LIBS
)

stress_target(stress_echo_server
    SOURCES net/stress_echo_server.cpp
    LIBS
)

find_library(EVENT_LIB event)
stress_target(stress_net_compare
    SOURCES net/stress_net_compare.cpp
    LIBS zero ${EVENT_LIB}
)

# libevent HTTP + wrk 已移除 — 统一用 C++ 客户端矩阵压测

include(FetchContent)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE
)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(spdlog)

stress_target(stress_log_matrix
    SOURCES log/stress_log_matrix.cpp
    LIBS zero spdlog::spdlog
)
