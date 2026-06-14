file(REMOVE_RECURSE
  "libnettycore_thread.a"
  "libnettycore_thread.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/nettycore_thread.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
