file(REMOVE_RECURSE
  "liblemo_socket.a"
  "liblemo_socket.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/lemo_socket.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
