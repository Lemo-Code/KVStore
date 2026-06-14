# KVStore CMake 辅助：分层模块测试与目标选项

if(NOT DEFINED KVSTORE_BIN_ROOT)
  set(KVSTORE_BIN_ROOT "${CMAKE_SOURCE_DIR}/bin" CACHE PATH "KVStore 可执行文件根目录")
endif()

if(NOT DEFINED KVSTORE_LIB_ROOT)
  set(KVSTORE_LIB_ROOT "${CMAKE_SOURCE_DIR}/lib" CACHE PATH "KVStore 动态库/静态库根目录")
endif()

set(LSTL_BIN_ROOT "${KVSTORE_BIN_ROOT}" CACHE PATH "兼容：可执行文件根目录")

set(KVSTORE_ALL_TEST_TARGETS "" CACHE INTERNAL "all kvstore test executables")
set(LSTL_ALL_TEST_TARGETS "${KVSTORE_ALL_TEST_TARGETS}" CACHE INTERNAL "")

function(kvstore_apply_target_options target_name)
  find_package(Threads REQUIRED)
  target_link_libraries(${target_name} PRIVATE Threads::Threads)
  if(LSTL_OOM_MODE_CERR)
    target_compile_definitions(${target_name} PRIVATE LSTL_OOM_MODE_CERR)
  endif()
endfunction()

function(lstl_apply_target_options target_name)
  kvstore_apply_target_options(${target_name})
endfunction()

# kvstore_add_tests(<module_path> [LIBS lib1 ...] [INCLUDES dir1 ...] TESTS t1 ...)
function(kvstore_add_tests MODULE_PATH)
  set(options "")
  set(oneValueArgs "")
  set(multiValueArgs LIBS INCLUDES TESTS)
  cmake_parse_arguments(KVS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT KVS_TESTS)
    message(FATAL_ERROR "kvstore_add_tests(${MODULE_PATH}): 需要 TESTS 列表")
  endif()

  string(REPLACE "/" "." ctest_prefix "${MODULE_PATH}")
  set(module_bin "${KVSTORE_BIN_ROOT}/${MODULE_PATH}")
  file(MAKE_DIRECTORY "${module_bin}")

  foreach(test_name ${KVS_TESTS})
    set(test_src "${CMAKE_SOURCE_DIR}/tests/${MODULE_PATH}/${test_name}.cc")
    if(NOT EXISTS "${test_src}")
      message(FATAL_ERROR "kvstore_add_tests: 找不到 ${test_src}")
    endif()

    add_executable(${test_name} "${test_src}")
    set_target_properties(${test_name} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${module_bin}"
      RUNTIME_OUTPUT_DIRECTORY_DEBUG "${module_bin}"
      RUNTIME_OUTPUT_DIRECTORY_RELEASE "${module_bin}"
      RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${module_bin}"
      RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${module_bin}"
    )
    kvstore_apply_target_options(${test_name})

    if(KVS_LIBS)
      target_link_libraries(${test_name} PRIVATE ${KVS_LIBS})
    endif()
    if(KVS_INCLUDES)
      target_include_directories(${test_name} PRIVATE ${KVS_INCLUDES})
    endif()

    add_test(NAME "${ctest_prefix}.${test_name}" COMMAND "${module_bin}/${test_name}")
    list(APPEND KVSTORE_ALL_TEST_TARGETS ${test_name})
    set(KVSTORE_ALL_TEST_TARGETS "${KVSTORE_ALL_TEST_TARGETS}" CACHE INTERNAL "" FORCE)
    set(LSTL_ALL_TEST_TARGETS "${KVSTORE_ALL_TEST_TARGETS}" CACHE INTERNAL "" FORCE)
  endforeach()
endfunction()

# 兼容旧接口
function(lstl_add_module_tests MODULE)
  set(options "")
  set(oneValueArgs "")
  set(multiValueArgs TESTS)
  cmake_parse_arguments(LST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  set(module_path "${MODULE}")
  if(MODULE STREQUAL "memory")
    set(module_path "lstl/memory")
  elseif(MODULE STREQUAL "container")
    set(module_path "lstl/container")
  elseif(MODULE STREQUAL "lsmtree")
    set(module_path "storage/lsmtree")
  endif()

  set(libs "")
  if(TARGET lstl_memory)
    list(APPEND libs lstl_memory)
  endif()
  if(MODULE STREQUAL "lsmtree" AND TARGET storage_lsmtree)
    list(APPEND libs storage_lsmtree)
  endif()

  kvstore_add_tests("${module_path}"
    LIBS ${libs}
    INCLUDES "${CMAKE_SOURCE_DIR}/tests/common"
    TESTS ${LST_TESTS})
endfunction()
