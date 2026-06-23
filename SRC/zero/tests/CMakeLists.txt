# zero test suite
# Organized into: base/thread/fiber/scheduler/net tests, bug regression, concurrency stress

set(ZERO_TEST_SOURCES
    # Base tests
    test_endian.cpp
    test_lexical_cast.cpp
    test_singleton.cpp
    test_any.cpp
    test_optional.cpp
    test_expected.cpp

    # Thread tests
    test_thread.cpp
    test_spinlock.cpp
    test_mutex.cpp
    test_rwlock.cpp
    test_semaphore.cpp
    test_condition_variable.cpp
    test_barrier.cpp
    test_cpu_affinity.cpp

    # Fiber tests
    test_fiber.cpp
    test_fiber_pool.cpp
    test_stack_pool.cpp
    test_fiber_local.cpp
    test_channel.cpp
    test_fiber_mutex.cpp

    # Scheduler tests
    test_scheduler.cpp
    test_timer_wheel.cpp
    test_work_stealing_queue.cpp
    test_fd_manager.cpp
    test_reactor.cpp

    # Network tests
    test_buffer.cpp
    test_address.cpp
    test_socket.cpp
    test_stream.cpp
    test_tcp.cpp
    test_udp.cpp
    test_tcp_echo.cpp

    # Log tests
    test_log.cpp

    # Config tests
    test_config.cpp

    # Bug regression tests
    test_bug_regression.cpp

    # Concurrency stress tests
    test_concurrent_fiber.cpp
    test_concurrent_scheduler.cpp
    test_concurrent_net.cpp
    test_concurrent_general.cpp

    # Performance benchmark tests
    test_bench_log.cpp
    test_bench_net.cpp
    test_bench_concurrent_net.cpp
)

set(TEST_LIBS zero GTest::gtest GTest::gtest_main Threads::Threads)

foreach(test_file ${ZERO_TEST_SOURCES})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} PRIVATE ${TEST_LIBS})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )
    target_compile_options(${test_name} PRIVATE -Wall -Wextra)
    add_test(NAME zero_${test_name} COMMAND ${test_name})
endforeach()

# Combined test runner for CI
add_executable(zero_all_tests ${ZERO_TEST_SOURCES})
target_link_libraries(zero_all_tests PRIVATE ${TEST_LIBS})
target_include_directories(zero_all_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)
add_test(NAME zero_all COMMAND zero_all_tests)
