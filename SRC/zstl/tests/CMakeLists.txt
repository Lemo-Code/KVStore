# zstl test suite
# Organized into: unit tests per container, bug regression, concurrency stress

set(ZSTL_TEST_SOURCES
    # Memory layer tests
    test_type_traits.cpp
    test_utility.cpp
    test_construct.cpp
    test_pool.cpp
    test_allocator.cpp
    test_uninitialized.cpp
    test_smart_ptr.cpp
    test_functional.cpp

    # Iterator tests
    test_iterator.cpp
    test_reverse_iterator.cpp
    test_move_iterator.cpp
    test_insert_iterator.cpp

    # Algorithm tests
    test_sort.cpp
    test_find.cpp
    test_algorithm.cpp

    # Sequential container tests
    test_vector.cpp
    test_list.cpp
    test_slist.cpp
    test_deque.cpp
    test_string.cpp
    test_array.cpp
    test_bitset.cpp

    # Associative container tests
    test_map.cpp
    test_set.cpp
    test_multimap.cpp
    test_multiset.cpp
    test_unordered_map.cpp
    test_unordered_set.cpp
    test_unordered_multimap.cpp
    test_unordered_multiset.cpp
    test_bmap.cpp
    test_bset.cpp
    test_skip_map.cpp
    test_skip_set.cpp

    # Container adaptor tests
    test_stack.cpp
    test_queue.cpp
    test_priority_queue.cpp

    # String utility tests
    test_string_view.cpp

    # Thread tests
    test_atomic.cpp
    test_mutex.cpp

    # Bug regression tests (comprehensive edge cases)
    test_bug_regression.cpp

    # Concurrency stress tests
    test_concurrent_vector.cpp
    test_concurrent_map.cpp
    test_concurrent_pool.cpp
    test_concurrent_general.cpp

    # Performance benchmark tests
    test_bench_pool.cpp
    test_bench_container.cpp
    test_bench_algorithm.cpp
)

# Create test executables — one per category to keep build fast
# Each test links against zstl and GTest

set(TEST_LIBS zstl GTest::gtest GTest::gtest_main Threads::Threads)

foreach(test_file ${ZSTL_TEST_SOURCES})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} PRIVATE ${TEST_LIBS})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_compile_options(${test_name} PRIVATE -Wall -Wextra)
    add_test(NAME zstl_${test_name} COMMAND ${test_name})
endforeach()

# Also create a combined test runner for CI
add_executable(zstl_all_tests ${ZSTL_TEST_SOURCES})
target_link_libraries(zstl_all_tests PRIVATE ${TEST_LIBS})
target_include_directories(zstl_all_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
add_test(NAME zstl_all COMMAND zstl_all_tests)
