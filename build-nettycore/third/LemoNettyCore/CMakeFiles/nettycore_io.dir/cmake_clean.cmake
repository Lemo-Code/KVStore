file(REMOVE_RECURSE
  "libnettycore_io.a"
  "libnettycore_io.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/nettycore_io.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
