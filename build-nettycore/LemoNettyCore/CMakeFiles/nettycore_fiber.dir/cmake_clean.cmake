file(REMOVE_RECURSE
  "libnettycore_fiber.a"
  "libnettycore_fiber.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/nettycore_fiber.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
