file(REMOVE_RECURSE
  "liblemo_utils.a"
  "liblemo_utils.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/lemo_utils.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
