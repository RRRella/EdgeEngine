#----------------------------------------------------------------
# Generated CMake target import file for configuration "MinSizeRel".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "GPUOpen::D3D12MemoryAllocator" for configuration "MinSizeRel"
set_property(TARGET GPUOpen::D3D12MemoryAllocator APPEND PROPERTY IMPORTED_CONFIGURATIONS MINSIZEREL)
set_target_properties(GPUOpen::D3D12MemoryAllocator PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_MINSIZEREL "CXX"
  IMPORTED_LOCATION_MINSIZEREL "${_IMPORT_PREFIX}/lib/D3D12MAs.lib"
  )

list(APPEND _cmake_import_check_targets GPUOpen::D3D12MemoryAllocator )
list(APPEND _cmake_import_check_files_for_GPUOpen::D3D12MemoryAllocator "${_IMPORT_PREFIX}/lib/D3D12MAs.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
