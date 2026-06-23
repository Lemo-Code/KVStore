# gtest.cmake - Fetch Google Test via FetchContent
# Google Test is the testing framework for all KVStore components.
# This module downloads gtest at configure time - no manual installation needed.

include(FetchContent)

# Prevent gtest from installing itself alongside our project
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
)

# Suppress gtest's own warnings in our build output
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Mark as populated if already done (prevents re-download)
if(NOT TARGET gtest)
    FetchContent_MakeAvailable(googletest)
endif()

# Ensure gtest uses the same C++ standard as the project
set_target_properties(gtest gtest_main PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
