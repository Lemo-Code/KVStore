file(REMOVE_RECURSE
  "libnetcore_io.a"
  "libnetcore_io.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/netcore_io.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
