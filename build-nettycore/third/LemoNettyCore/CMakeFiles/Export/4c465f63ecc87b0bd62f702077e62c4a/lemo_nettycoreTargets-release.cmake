#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "lemo::lemo_nettycore" for configuration "Release"
set_property(TARGET lemo::lemo_nettycore APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(lemo::lemo_nettycore PROPERTIES
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/liblemo_nettycore.so.0.4.0"
  IMPORTED_SONAME_RELEASE "liblemo_nettycore.so.0"
  )

list(APPEND _cmake_import_check_targets lemo::lemo_nettycore )
list(APPEND _cmake_import_check_files_for_lemo::lemo_nettycore "${_IMPORT_PREFIX}/lib/liblemo_nettycore.so.0.4.0" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
