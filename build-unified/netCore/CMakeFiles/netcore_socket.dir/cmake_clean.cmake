file(REMOVE_RECURSE
  "libnetcore_socket.a"
  "libnetcore_socket.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/netcore_socket.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
