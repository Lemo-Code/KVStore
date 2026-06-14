file(REMOVE_RECURSE
  "libnettycore_log.a"
  "libnettycore_log.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/nettycore_log.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
