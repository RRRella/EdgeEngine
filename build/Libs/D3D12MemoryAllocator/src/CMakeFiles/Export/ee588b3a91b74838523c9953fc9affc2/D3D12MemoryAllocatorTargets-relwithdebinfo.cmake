#----------------------------------------------------------------
# Generated CMake target import file for configuration "RelWithDebInfo".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "GPUOpen::D3D12MemoryAllocator" for configuration "RelWithDebInfo"
set_property(TARGET GPUOpen::D3D12MemoryAllocator APPEND PROPERTY IMPORTED_CONFIGURATIONS RELWITHDEBINFO)
set_target_properties(GPUOpen::D3D12MemoryAllocator PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELWITHDEBINFO "CXX"
  IMPORTED_LOCATION_RELWITHDEBINFO "${_IMPORT_PREFIX}/lib/D3D12MArd.lib"
  )

list(APPEND _cmake_import_check_targets GPUOpen::D3D12MemoryAllocator )
list(APPEND _cmake_import_check_files_for_GPUOpen::D3D12MemoryAllocator "${_IMPORT_PREFIX}/lib/D3D12MArd.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
