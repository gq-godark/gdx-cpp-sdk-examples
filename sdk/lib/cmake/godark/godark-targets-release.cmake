#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "godark::godark" for configuration "Release"
set_property(TARGET godark::godark APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(godark::godark PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libgodark.a"
  )

list(APPEND _cmake_import_check_targets godark::godark )
list(APPEND _cmake_import_check_files_for_godark::godark "${_IMPORT_PREFIX}/lib/libgodark.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
