# TestUtils.cmake - Helper macros for adding tests across all KVStore components
#
# Each macro handles:
#   - Creating the test executable with correct source file
#   - Linking to gtest + gtest_main + the component library
#   - Setting include directories
#   - Registering with CTest under a component-namespaced name

# ---- Zero Library Tests ----
macro(add_zero_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE zero gtest gtest_main pthread dl yaml-cpp)
    target_include_directories(${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )
    add_test(NAME zero.${name} COMMAND ${name})
endmacro()

# ---- LSTL Library Tests ----
macro(add_lstl_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE lstl gtest gtest_main)
    add_test(NAME lstl.${name} COMMAND ${name})
endmacro()

# ---- LRPC Library Tests ----
macro(add_lrpc_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE gtest gtest_main pthread)
    target_include_directories(${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/lstl/include
    )
    add_test(NAME lrpc.${name} COMMAND ${name})
endmacro()

# ---- Ledis Library Tests ----
macro(add_ledis_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE ledis gtest gtest_main pthread dl yaml-cpp)
    target_include_directories(${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/lstl/include
    )
    add_test(NAME ledis.${name} COMMAND ${name})
endmacro()

# ---- Integration Tests ----
macro(add_integration_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE ledis gtest gtest_main pthread dl yaml-cpp)
    target_include_directories(${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/lstl/include
    )
    add_test(NAME integration.${name} COMMAND ${name})
endmacro()

# ---- Benchmark Tests (not registered with CTest by default) ----
macro(add_benchmark name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE ${ARGN} gtest gtest_main pthread dl yaml-cpp)
    target_include_directories(${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/lstl/include
    )
    # Benchmarks are not added to CTest; run manually
endmacro()

# ---- Stress Tests (registered under "stress." prefix) ----
macro(add_stress_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE ledis gtest gtest_main pthread dl yaml-cpp)
    target_include_directories(${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/lstl/include
    )
    add_test(NAME stress.${name} COMMAND ${name})
endmacro()
