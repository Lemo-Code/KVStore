file(REMOVE_RECURSE
  "liblemo_memory.a"
  "liblemo_memory.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/lemo_memory.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
