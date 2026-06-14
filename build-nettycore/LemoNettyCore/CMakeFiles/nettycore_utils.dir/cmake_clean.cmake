file(REMOVE_RECURSE
  "libnettycore_utils.a"
  "libnettycore_utils.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/nettycore_utils.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
