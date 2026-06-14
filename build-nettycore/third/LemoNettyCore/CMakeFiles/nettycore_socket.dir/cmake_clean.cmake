file(REMOVE_RECURSE
  "libnettycore_socket.a"
  "libnettycore_socket.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/nettycore_socket.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
