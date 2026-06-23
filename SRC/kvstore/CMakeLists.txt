cmake_minimum_required(VERSION 3.14)
project(kvstore VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================
# kvstore — distributed KV system built on zero + zstl
# ============================================================
set(KVSTORE_SOURCES
    # Common
    common/kv_utils.cc

    # Config
    config/kv_config.cc

    # Protocol
    protocol/kv_codec.cc

    # Storage
    storage/memory_engine.cc
    storage/wal_writer.cc
    storage/wal_reader.cc
    storage/sstable_builder.cc
    storage/sstable_reader.cc
    storage/compactor.cc

    # Raft
    raft/raft_types.cc
    raft/raft_log.cc
    raft/raft_node.cc
    raft/raft_election.cc
    raft/raft_replication.cc
    raft/raft_snapshot.cc
    raft/raft_membership.cc
    raft/raft_transport.cc

    # Shard
    shard/consistent_hash.cc
    shard/shard_registry.cc
    shard/shard_controller.cc
    shard/shard_migration.cc

    # API
    api/kv_api.cc
    api/txn_context.cc

    # Server
    server/kv_server.cc
    server/client_session.cc

    # Client
    client/kv_client.cc

    # Monitor
    monitor/health_checker.cc
    monitor/metrics_collector.cc
)

add_library(kvstore STATIC ${KVSTORE_SOURCES})

target_include_directories(kvstore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../zero/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../zstl/include
)

target_link_libraries(kvstore PUBLIC
    pthread
    yaml-cpp
)

# Compiler options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(kvstore PRIVATE -Wall -Wextra)
endif()

# ============================================================
# kvstore server binary
# ============================================================
option(BUILD_KVSTORE_SERVER "Build kvstore server binary" ON)
if(BUILD_KVSTORE_SERVER)
    add_executable(kvstore-server server/kv_main.cc)
    target_link_libraries(kvstore-server PRIVATE kvstore)
endif()

# ============================================================
# Tests
# ============================================================
option(BUILD_KVSTORE_TESTS "Build kvstore tests" ON)
if(BUILD_KVSTORE_TESTS)
    add_executable(kvstore-test server/test_runner.cc)
    target_link_libraries(kvstore-test PRIVATE kvstore)
endif()
