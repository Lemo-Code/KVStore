set(NET_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(net_log STATIC
  "${NET_ROOT}/util.cc"
  "${NET_ROOT}/log/level.cc"
  "${NET_ROOT}/log/event.cc"
  "${NET_ROOT}/log/wrap.cc"
  "${NET_ROOT}/log/formatter.cc"
  "${NET_ROOT}/log/file_sink.cc"
  "${NET_ROOT}/log/appender.cc"
  "${NET_ROOT}/log/sink.cc"
  "${NET_ROOT}/log/file_writer.cc"
  "${NET_ROOT}/log/async_sink.cc"
  "${NET_ROOT}/log/logger.cc"
  "${NET_ROOT}/log/manager.cc"
)
target_include_directories(net_log PUBLIC "${NET_ROOT}")
target_compile_features(net_log PUBLIC cxx_std_11)
find_package(Threads REQUIRED)
target_link_libraries(net_log PUBLIC Threads::Threads)

set(NET_LOG_TESTS
  test_singleton_util
  test_log_level
  test_log_formatter
  test_log_sync
  test_log_async
  test_log_async_boundary
  test_log_appender
)

set(net_bin "${LSTL_BIN_ROOT}/net")
file(MAKE_DIRECTORY "${net_bin}")

foreach(test_name ${NET_LOG_TESTS})
  add_executable(${test_name}
    "${CMAKE_SOURCE_DIR}/tests/net/${test_name}.cc")
  target_link_libraries(${test_name} PRIVATE net_log)
  set_target_properties(${test_name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${net_bin}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${net_bin}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${net_bin}"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${net_bin}"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${net_bin}"
  )
  add_test(NAME "net.${test_name}" COMMAND "${net_bin}/${test_name}")
  list(APPEND NET_LOG_ALL_TEST_TARGETS ${test_name})
endforeach()

set(NET_LOG_ALL_TEST_TARGETS "${NET_LOG_ALL_TEST_TARGETS}" PARENT_SCOPE)
