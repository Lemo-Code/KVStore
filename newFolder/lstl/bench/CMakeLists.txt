# lstl benchmark targets

function(add_lstl_bench bench_name bench_source)
    add_executable(${bench_name} ${bench_source})
    target_link_libraries(${bench_name} PRIVATE lstl Threads::Threads)
    # Benchmarks use -O2 for fair comparison with std::
    target_compile_options(${bench_name} PRIVATE -O2 -DNDEBUG)
endfunction()

add_lstl_bench(bench_vector bench_vector.cpp)
add_lstl_bench(bench_map bench_map.cpp)
add_lstl_bench(bench_pool bench_pool.cpp)
add_lstl_bench(bench_concurrent bench_concurrent.cpp)
add_lstl_bench(bench_all bench_all.cpp)
add_lstl_bench(bench_sizes bench_sizes.cpp)
