file(REMOVE_RECURSE
  ".0"
  "liblemo_nettycore.pdb"
  "liblemo_nettycore.so"
  "liblemo_nettycore.so.0"
  "liblemo_nettycore.so.0.4.0"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/lemo_nettycore.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
