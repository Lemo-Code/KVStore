# lstl test targets
#
# Each test file is a standalone executable that links to lstl (header-only).

# Helper function to add a test
function(add_lstl_test test_name test_source)
    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name} PRIVATE lstl)
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

# Memory subsystem tests
add_lstl_test(test_alloc test_alloc.cpp)
add_lstl_test(test_pool test_pool.cpp)
add_lstl_test(test_construct test_construct.cpp)
add_lstl_test(test_uninitialized test_uninitialized.cpp)
add_lstl_test(test_type_traits test_type_traits.cpp)
add_lstl_test(test_utility test_utility.cpp)

# Container tests
add_lstl_test(test_vector test_vector.cpp)
add_lstl_test(test_list test_list.cpp)
add_lstl_test(test_slist test_slist.cpp)
add_lstl_test(test_deque test_deque.cpp)
add_lstl_test(test_stack_queue test_stack_queue.cpp)
add_lstl_test(test_map_set test_map_set.cpp)
add_lstl_test(test_unordered test_unordered.cpp)
add_lstl_test(test_skip test_skip.cpp)
add_lstl_test(test_bplus test_bplus.cpp)
