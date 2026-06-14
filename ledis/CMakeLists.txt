cmake_minimum_required(VERSION 3.10)

set(LEDIS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(LEDIS_INCLUDE "${LEDIS_ROOT}/include")
set(NETTYCORE_INCLUDE "${CMAKE_SOURCE_DIR}/third/LemoNettyCore/include")

if(NOT TARGET nettycore_buffer)
  message(FATAL_ERROR "Ledis 需要 LEMONETTYCORE_BUILD=ON")
endif()

find_package(Threads REQUIRED)
find_package(yaml-cpp REQUIRED)

function(ledis_apply_flags target_name)
  target_compile_features(${target_name} PUBLIC cxx_std_11)
  if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_options(${target_name} PRIVATE -O3 -DNDEBUG)
  endif()
endfunction()

function(ledis_link_nettycore target_name)
  target_link_libraries(${target_name} PUBLIC nettycore)
  target_include_directories(${target_name} PRIVATE "${NETTYCORE_INCLUDE}")
endfunction()

set(LEDIS_PROTOCOL_SRCS
  "${LEDIS_ROOT}/src/store/sds.cc"
  "${LEDIS_ROOT}/src/protocol/resp_value.cc"
  "${LEDIS_ROOT}/src/protocol/resp_reader.cc"
  "${LEDIS_ROOT}/src/protocol/resp_writer.cc"
  "${LEDIS_ROOT}/src/command/command_result.cc")

add_library(ledis_protocol STATIC ${LEDIS_PROTOCOL_SRCS})
target_include_directories(ledis_protocol PUBLIC "${LEDIS_INCLUDE}")
target_include_directories(ledis_protocol PRIVATE "${NETTYCORE_INCLUDE}")
target_link_libraries(ledis_protocol PUBLIC nettycore_buffer)
ledis_apply_flags(ledis_protocol)

set(LEDIS_STORE_SRCS
  "${LEDIS_ROOT}/src/store/object.cc"
  "${LEDIS_ROOT}/src/store/keyspace.cc"
  "${LEDIS_ROOT}/src/store/db_manager.cc"
  "${LEDIS_ROOT}/src/store/memory.cc"
  "${LEDIS_ROOT}/src/store/eviction.cc"
  "${LEDIS_ROOT}/src/store/snapshot.cc"
  "${LEDIS_ROOT}/src/store/aof.cc")

add_library(ledis_store STATIC ${LEDIS_STORE_SRCS})
target_link_libraries(ledis_store PUBLIC ledis_protocol lstl_memory)
target_include_directories(ledis_store PUBLIC "${LEDIS_INCLUDE}")
target_include_directories(ledis_store PRIVATE "${NETTYCORE_INCLUDE}")
ledis_apply_flags(ledis_store)

add_library(ledis_command STATIC "${LEDIS_ROOT}/src/command/registry.cc")
target_link_libraries(ledis_command PUBLIC ledis_store)
target_include_directories(ledis_command PUBLIC "${LEDIS_INCLUDE}")
ledis_apply_flags(ledis_command)

add_library(ledis_stream STATIC "${LEDIS_ROOT}/src/stream/ledis_stream.cc")
target_link_libraries(ledis_stream PUBLIC ledis_protocol)
target_include_directories(ledis_stream PUBLIC "${LEDIS_INCLUDE}")
target_include_directories(ledis_stream PRIVATE "${NETTYCORE_INCLUDE}")
ledis_apply_flags(ledis_stream)

set(LEDIS_SESSION_SRCS
  "${LEDIS_ROOT}/src/session/session.cc"
  "${LEDIS_ROOT}/src/session/inbound_queue.cc"
  "${LEDIS_ROOT}/src/session/reply_router.cc"
  "${LEDIS_ROOT}/src/session/db_worker.cc"
  "${LEDIS_ROOT}/src/session/ledis_engine.cc"
  "${LEDIS_ROOT}/src/session/io_worker.cc"
  "${LEDIS_ROOT}/src/session/pubsub.cc"
  "${LEDIS_ROOT}/src/session/blocking_list.cc")

add_library(ledis_session STATIC ${LEDIS_SESSION_SRCS})
target_link_libraries(ledis_session PUBLIC ledis_stream ledis_command)
target_include_directories(ledis_session PUBLIC "${LEDIS_INCLUDE}")
target_include_directories(ledis_session PRIVATE "${NETTYCORE_INCLUDE}")
ledis_apply_flags(ledis_session)

set(LEDIS_APP_SRCS
  "${LEDIS_ROOT}/src/app/env.cc"
  "${LEDIS_ROOT}/src/app/fs_util.cc"
  "${LEDIS_ROOT}/src/app/daemon.cc"
  "${LEDIS_ROOT}/src/app/application.cc")

add_library(ledis_server_lib STATIC
  ${LEDIS_APP_SRCS}
  "${LEDIS_ROOT}/src/config/ledis_settings.cc"
  "${LEDIS_ROOT}/src/config/ledis_yaml_config.cc"
  "${LEDIS_ROOT}/src/server/ledis_server.cc")
target_link_libraries(ledis_server_lib PUBLIC ledis_session yaml-cpp)
ledis_link_nettycore(ledis_server_lib)
target_include_directories(ledis_server_lib PUBLIC "${LEDIS_INCLUDE}")
ledis_apply_flags(ledis_server_lib)

set(LEDIS_SERVER_BIN "${KVSTORE_BIN_ROOT}/Ledis")
file(MAKE_DIRECTORY "${LEDIS_SERVER_BIN}")
add_executable(ledis-server "${LEDIS_ROOT}/src/server/ledis_server_main.cc")
target_link_libraries(ledis-server PRIVATE ledis_server_lib)
target_include_directories(ledis-server PRIVATE "${LEDIS_INCLUDE}")
set_target_properties(ledis-server PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${LEDIS_SERVER_BIN}")
ledis_apply_flags(ledis-server)

# 构建时将 conf/ 同步到可执行文件旁，供 -c conf 与默认 conf/ledis.yaml 自动加载
add_custom_command(TARGET ledis-server POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${LEDIS_ROOT}/conf" "${LEDIS_SERVER_BIN}/conf"
  COMMENT "Copy ledis/conf -> bin/Ledis/conf")

add_library(ledis INTERFACE)
target_link_libraries(ledis INTERFACE ledis_server_lib ledis_session ledis_command
                                    ledis_store ledis_stream ledis_protocol)
target_include_directories(ledis INTERFACE "${LEDIS_INCLUDE}")

function(ledis_add_test module_path test_name)
  set(test_bin "${KVSTORE_BIN_ROOT}/Ledis/${module_path}")
  file(MAKE_DIRECTORY "${test_bin}")
  set(target "${test_name}_ledis")
  add_executable(${target}
    "${CMAKE_SOURCE_DIR}/tests/ledis/${module_path}/${test_name}.cc")
  target_link_libraries(${target} PRIVATE ledis_server_lib)
  target_include_directories(${target} PRIVATE
    "${LEDIS_INCLUDE}" "${NETTYCORE_INCLUDE}"
    "${CMAKE_SOURCE_DIR}/tests/ledis")
  set_target_properties(${target} PROPERTIES
    OUTPUT_NAME ${test_name}
    RUNTIME_OUTPUT_DIRECTORY "${test_bin}")
  ledis_apply_flags(${target})
  string(REPLACE "/" "." ctest_module "Ledis.${module_path}")
  add_test(NAME "${ctest_module}.${test_name}"
    COMMAND "${test_bin}/${test_name}")
  list(APPEND KVSTORE_ALL_TEST_TARGETS ${target})
  set(KVSTORE_ALL_TEST_TARGETS "${KVSTORE_ALL_TEST_TARGETS}" CACHE INTERNAL "" FORCE)
endfunction()

ledis_add_test(protocol test_resp_reader)
ledis_add_test(protocol test_resp_writer)
ledis_add_test(store test_keyspace)
ledis_add_test(store test_hash_object)
ledis_add_test(store test_list_object)
ledis_add_test(store test_set_object)
ledis_add_test(store test_zset_object)
ledis_add_test(store test_snapshot)
ledis_add_test(store test_aof)
ledis_add_test(command test_ping)
ledis_add_test(config test_ledis_yaml_config)
ledis_add_test(app test_env)
ledis_add_test(command test_phase3)
ledis_add_test(command test_hash_commands)
ledis_add_test(command test_list_commands)
ledis_add_test(command test_list_blocking)
ledis_add_test(command test_set_commands)
ledis_add_test(command test_zset_commands)
ledis_add_test(command test_phase6)
ledis_add_test(command test_key_commands)
ledis_add_test(command test_string_commands)
ledis_add_test(command test_multi_exec)
ledis_add_test(command test_maxmemory)
ledis_add_test(command test_pubsub_commands)
ledis_add_test(session test_command_queue)
ledis_add_test(session test_io_db)
ledis_add_test(session test_query_buffer)
ledis_add_test(stream test_ledis_stream)
ledis_add_test(session test_session)
ledis_add_test(integration test_mvp)
ledis_add_test(integration test_pipeline)
ledis_add_test(integration test_async_tcp)
ledis_add_test(integration test_maxclients)
ledis_add_test(integration test_rdb_restart)
ledis_add_test(integration test_aof_restart)

add_custom_target(run_tests_ledis
  COMMAND ${CMAKE_CTEST_COMMAND} -R "^Ledis\\." --output-on-failure
  DEPENDS test_resp_reader_ledis test_resp_writer_ledis test_keyspace_ledis
          test_hash_object_ledis test_list_object_ledis test_set_object_ledis
          test_zset_object_ledis test_snapshot_ledis test_aof_ledis test_ping_ledis
          test_ledis_yaml_config_ledis test_env_ledis test_phase3_ledis test_hash_commands_ledis test_list_commands_ledis
          test_list_blocking_ledis test_set_commands_ledis test_zset_commands_ledis test_phase6_ledis test_key_commands_ledis
          test_string_commands_ledis test_multi_exec_ledis test_maxmemory_ledis
          test_pubsub_commands_ledis test_command_queue_ledis
          test_io_db_ledis test_query_buffer_ledis test_ledis_stream_ledis
          test_session_ledis test_mvp_ledis test_pipeline_ledis
          test_async_tcp_ledis test_maxclients_ledis test_rdb_restart_ledis
          test_aof_restart_ledis
  COMMENT "运行 Ledis 全套测试")
