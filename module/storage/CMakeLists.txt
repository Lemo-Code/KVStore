# storage 层：存储引擎（header-only，依赖 lstl）

set(STORAGE_LSM_TREE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lsmtree")

add_library(storage_lsmtree INTERFACE)
add_library(storage::lsmtree ALIAS storage_lsmtree)
target_include_directories(storage_lsmtree INTERFACE ${STORAGE_LSM_TREE_ROOT})
target_link_libraries(storage_lsmtree INTERFACE lstl_memory)

if(LSTL_BUILD_LSM_TREE)
  kvstore_add_tests(storage/lsmtree
    LIBS storage_lsmtree lstl_memory
    INCLUDES "${CMAKE_SOURCE_DIR}/tests/common"
    TESTS test_lsm_tree test_lsm_persistent)
endif()
