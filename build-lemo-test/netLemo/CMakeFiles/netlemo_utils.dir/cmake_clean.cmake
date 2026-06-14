file(REMOVE_RECURSE
  "libnetlemo_utils.a"
  "libnetlemo_utils.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/netlemo_utils.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
