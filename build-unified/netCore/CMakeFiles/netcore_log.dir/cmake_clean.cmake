file(REMOVE_RECURSE
  "libnetcore_log.a"
  "libnetcore_log.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/netcore_log.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
