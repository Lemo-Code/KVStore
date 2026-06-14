file(REMOVE_RECURSE
  "liblemo_io.a"
  "liblemo_io.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/lemo_io.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
