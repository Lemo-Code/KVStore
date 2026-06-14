file(REMOVE_RECURSE
  "libnettycore_memory.a"
  "libnettycore_memory.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/nettycore_memory.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
