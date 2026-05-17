# LSTL CMake 辅助：按模块将可执行文件输出到 ${LSTL_BIN_ROOT}/<module>/

if(NOT DEFINED LSTL_BIN_ROOT)
  set(LSTL_BIN_ROOT "${CMAKE_SOURCE_DIR}/bin" CACHE PATH "LSTL 可执行文件根目录")
endif()

set(LSTL_ALL_TEST_TARGETS "" CACHE INTERNAL "all lstl test executables")

function(lstl_apply_target_options target_name)
  if(TARGET lstl_memory)
    target_link_libraries(${target_name} PRIVATE lstl_memory)
  endif()
  find_package(Threads REQUIRED)
  target_link_libraries(${target_name} PRIVATE Threads::Threads)
  if(LSTL_OOM_MODE_CERR)
    target_compile_definitions(${target_name} PRIVATE LSTL_OOM_MODE_CERR)
  endif()
endfunction()

# lstl_add_module_tests(<module> TESTS t1 t2 ...)
# 源文件查找顺序：
#   1. tests/<module>/<name>.cc
#   2. tests/<name>.cc（兼容旧布局）
function(lstl_add_module_tests MODULE)
  set(options "")
  set(oneValueArgs "")
  set(multiValueArgs TESTS)
  cmake_parse_arguments(LST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT LST_TESTS)
    message(FATAL_ERROR "lstl_add_module_tests(${MODULE}): 需要 TESTS 列表")
  endif()

  set(module_bin "${LSTL_BIN_ROOT}/${MODULE}")
  file(MAKE_DIRECTORY "${module_bin}")

  foreach(test_name ${LST_TESTS})
    set(test_src "${CMAKE_SOURCE_DIR}/tests/${MODULE}/${test_name}.cc")
    if(NOT EXISTS "${test_src}")
      set(test_src "${CMAKE_SOURCE_DIR}/tests/${test_name}.cc")
    endif()
    if(NOT EXISTS "${test_src}")
      message(FATAL_ERROR "lstl_add_module_tests: 找不到源文件 ${test_name}.cc "
                          "(已查找 tests/${MODULE}/ 与 tests/)")
    endif()

    add_executable(${test_name} "${test_src}")
    set_target_properties(${test_name} PROPERTIES
      RUNTIME_OUTPUT_DIRECTORY "${module_bin}"
      RUNTIME_OUTPUT_DIRECTORY_DEBUG "${module_bin}"
      RUNTIME_OUTPUT_DIRECTORY_RELEASE "${module_bin}"
      RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${module_bin}"
      RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${module_bin}"
    )
    lstl_apply_target_options(${test_name})

    set(exe_path "${module_bin}/${test_name}")
    add_test(NAME "${MODULE}.${test_name}" COMMAND "${exe_path}")

    list(APPEND LSTL_ALL_TEST_TARGETS ${test_name})
    set(LSTL_ALL_TEST_TARGETS "${LSTL_ALL_TEST_TARGETS}" CACHE INTERNAL "" FORCE)
  endforeach()
endfunction()
