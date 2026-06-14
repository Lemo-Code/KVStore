file(REMOVE_RECURSE
  "liblemo_buffer.a"
  "liblemo_buffer.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/lemo_buffer.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
