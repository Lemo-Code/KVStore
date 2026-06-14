file(REMOVE_RECURSE
  "libnettycore_server.a"
  "libnettycore_server.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/nettycore_server.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
