file(REMOVE_RECURSE
  "libnettycore.a"
  "libnettycore.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/nettycore.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
