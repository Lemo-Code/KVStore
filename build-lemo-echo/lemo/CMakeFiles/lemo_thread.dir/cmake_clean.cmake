file(REMOVE_RECURSE
  "liblemo_thread.a"
  "liblemo_thread.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/lemo_thread.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
