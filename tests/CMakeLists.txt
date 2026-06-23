# KVStore 统一测试入口 — 使用 Google Test

set(KVSTORE_TEST_COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/common)

function(kvstore_add_test target)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS;INCLUDE_DIRS" ${ARGN})
    if(NOT ARG_SOURCES)
        set(ARG_SOURCES ${target}.cpp)
    endif()

    add_executable(${target} ${ARG_SOURCES})
    target_include_directories(${target} PRIVATE
        ${KVSTORE_TEST_COMMON_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/lstl/include
        ${ARG_INCLUDE_DIRS}
    )
    target_link_libraries(${target} PRIVATE
        gtest_main
        ${ARG_LIBS}
    )
    if(ENABLE_COVERAGE)
        target_compile_options(${target} PRIVATE --coverage -fprofile-arcs -ftest-coverage)
        target_link_options(${target} PRIVATE --coverage)
    endif()
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES TIMEOUT 120)
endfunction()

add_subdirectory(zero)
add_subdirectory(lstl)
add_subdirectory(ledis)
add_subdirectory(lrpc)
add_subdirectory(integration)
