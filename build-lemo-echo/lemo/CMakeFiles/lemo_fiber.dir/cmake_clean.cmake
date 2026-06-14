file(REMOVE_RECURSE
  "liblemo_fiber.a"
  "liblemo_fiber.pdb"
)

# Per-language clean rules from dependency scanning.
foreach(lang CXX)
  include(CMakeFiles/lemo_fiber.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
