# cmake/coverage.cmake — gcov/lcov 代码覆盖率支持
option(ENABLE_COVERAGE "Enable gcov code coverage instrumentation" OFF)

if(ENABLE_COVERAGE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "Coverage requires GCC or Clang")
    endif()
    message(STATUS "Code coverage enabled (-fprofile-arcs -ftest-coverage)")
    add_compile_options(--coverage -fprofile-arcs -ftest-coverage -O0 -g)
    add_link_options(--coverage)
endif()
