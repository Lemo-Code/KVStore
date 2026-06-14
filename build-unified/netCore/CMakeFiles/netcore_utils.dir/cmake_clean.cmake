file(REMOVE_RECURSE
  "libnetcore_utils.a"
  "libnetcore_utils.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/netcore_utils.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
