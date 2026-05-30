# lstl 层：空间配置 + 容器（header-only）

set(LSTL_MEMORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/memory")
set(LSTL_CONTAINER_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/container")

add_library(lstl_memory INTERFACE)
add_library(lstl::memory ALIAS lstl_memory)
target_include_directories(lstl_memory INTERFACE
  ${LSTL_MEMORY_ROOT}
  ${LSTL_CONTAINER_ROOT})

if(LSTL_OOM_MODE_CERR)
  target_compile_definitions(lstl_memory INTERFACE LSTL_OOM_MODE_CERR)
endif()
if(LSTL_USE_MALLOC_ALLOC)
  target_compile_definitions(lstl_memory INTERFACE LSTL_USE_MALLOC_ALLOC=1)
endif()
if(LSTL_POOL_TRACK_SPAN)
  target_compile_definitions(lstl_memory INTERFACE LSTL_POOL_LIGHT=0)
endif()

set(LSTL_MEMORY_TESTS
  test_alloc test_construct test_uninitialized test_allocator
  test_temporary_buffer test_type_traits test_oom_policy
  test_pool_single test_algorithm
)
if(LSTL_BUILD_MEMORY_STRESS)
  list(APPEND LSTL_MEMORY_TESTS test_memory_stress)
endif()

kvstore_add_tests(lstl/memory
  LIBS lstl_memory
  INCLUDES "${CMAKE_SOURCE_DIR}/tests/common"
  TESTS ${LSTL_MEMORY_TESTS})

if(LSTL_BUILD_CONTAINER)
  kvstore_add_tests(lstl/container
    LIBS lstl_memory
    INCLUDES "${CMAKE_SOURCE_DIR}/tests/common"
    TESTS
      test_vector test_list test_deque test_slist
      test_heap test_adapter test_associative test_b_associative
      test_skip_associative)
endif()
