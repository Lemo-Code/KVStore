file(REMOVE_RECURSE
  "libnetcore_thread.a"
  "libnetcore_thread.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/netcore_thread.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
