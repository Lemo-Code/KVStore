file(REMOVE_RECURSE
  "libnetlemo_net.a"
  "libnetlemo_net.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/netlemo_net.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
