file(REMOVE_RECURSE
  "libnetcore_memory.a"
  "libnetcore_memory.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/netcore_memory.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
