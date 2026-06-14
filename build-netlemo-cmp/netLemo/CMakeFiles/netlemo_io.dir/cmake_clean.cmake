file(REMOVE_RECURSE
  "libnetlemo_io.a"
  "libnetlemo_io.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/netlemo_io.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
