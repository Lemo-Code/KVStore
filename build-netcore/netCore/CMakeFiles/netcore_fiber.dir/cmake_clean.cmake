file(REMOVE_RECURSE
  "libnetcore_fiber.a"
  "libnetcore_fiber.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/netcore_fiber.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
