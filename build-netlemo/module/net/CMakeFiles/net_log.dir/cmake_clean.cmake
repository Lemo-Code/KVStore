file(REMOVE_RECURSE
  "libnet_log.a"
  "libnet_log.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/net_log.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
