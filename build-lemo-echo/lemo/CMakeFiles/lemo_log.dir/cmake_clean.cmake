file(REMOVE_RECURSE
  "liblemo_log.a"
  "liblemo_log.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/lemo_log.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
