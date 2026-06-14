file(REMOVE_RECURSE
  "libnetcore_buffer.a"
  "libnetcore_buffer.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/netcore_buffer.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
