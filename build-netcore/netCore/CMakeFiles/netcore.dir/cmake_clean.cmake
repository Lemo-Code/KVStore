file(REMOVE_RECURSE
  "libnetcore.a"
  "libnetcore.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/netcore.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
