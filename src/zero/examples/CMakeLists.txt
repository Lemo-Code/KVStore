enable_language(ASM)

add_executable(fiber_test fiber_test.cc)
target_link_libraries(fiber_test zero)

add_executable(ctx_test ctx_test.cc)
target_link_libraries(ctx_test zero)

add_executable(timer_test timer_test.cc)
target_link_libraries(timer_test zero)

add_executable(reactor_test reactor_test.cc)
target_link_libraries(reactor_test zero)

add_executable(net_test net_test.cc)
target_link_libraries(net_test zero)

add_executable(echo_server echo_server.cc)
target_link_libraries(echo_server zero)

add_executable(echo_minimal echo_minimal.cc)
target_link_libraries(echo_minimal zero)

add_executable(http_server http_server.cc)
target_link_libraries(http_server zero)

add_executable(epoll_test epoll_test.cc)

add_executable(stress_test stress_test.cc)
target_link_libraries(stress_test zero)

add_executable(log_bench log_bench.cc)
target_link_libraries(log_bench zero)

add_executable(config_test config_test.cc)
target_link_libraries(config_test zero)

# Benchmark
add_executable(bench_echo bench_echo.cc)
target_link_libraries(bench_echo zero)

add_executable(stress_client stress_client.cc)
target_link_libraries(stress_client pthread)

add_executable(stress_client_mt stress_client_mt.cc)
target_link_libraries(stress_client_mt pthread)
