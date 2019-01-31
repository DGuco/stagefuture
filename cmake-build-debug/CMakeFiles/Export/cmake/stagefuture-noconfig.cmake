#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "stagefuture" for configuration ""
set_property(TARGET stagefuture APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(stagefuture PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libstagefuture.so"
  IMPORTED_SONAME_NOCONFIG "libstagefuture.so"
  )

list(APPEND _IMPORT_CHECK_TARGETS stagefuture )
list(APPEND _IMPORT_CHECK_FILES_FOR_stagefuture "${_IMPORT_PREFIX}/lib/libstagefuture.so" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
