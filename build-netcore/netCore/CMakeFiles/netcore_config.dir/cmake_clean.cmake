file(REMOVE_RECURSE
  "libnetcore_config.a"
  "libnetcore_config.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/netcore_config.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
