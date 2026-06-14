file(REMOVE_RECURSE
  "liblemo_config.a"
  "liblemo_config.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/lemo_config.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
