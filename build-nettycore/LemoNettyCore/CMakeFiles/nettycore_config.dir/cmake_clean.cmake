file(REMOVE_RECURSE
  "libnettycore_config.a"
  "libnettycore_config.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/nettycore_config.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
