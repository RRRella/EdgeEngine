# Install script for directory: D:/Dev/Local/EdgeEngine/Libs/D3D12MemoryAllocator/src

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "D:/Program Files/Edgine")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/cmake/D3D12MemoryAllocator" TYPE FILE FILES
    "D:/Dev/Local/EdgeEngine/build/Libs/D3D12MemoryAllocator/src/generated/D3D12MemoryAllocatorConfig.cmake"
    "D:/Dev/Local/EdgeEngine/build/Libs/D3D12MemoryAllocator/src/generated/D3D12MemoryAllocatorConfigVersion.cmake"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/cmake/D3D12MemoryAllocator/D3D12MemoryAllocatorTargets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/cmake/D3D12MemoryAllocator/D3D12MemoryAllocatorTargets.cmake"
         "D:/Dev/Local/EdgeEngine/build/Libs/D3D12MemoryAllocator/src/CMakeFiles/Export/ee588b3a91b74838523c9953fc9affc2/D3D12MemoryAllocatorTargets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/cmake/D3D12MemoryAllocator/D3D12MemoryAllocatorTargets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/cmake/D3D12MemoryAllocator/D3D12MemoryAllocatorTargets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/cmake/D3D12MemoryAllocator" TYPE FILE FILES "D:/Dev/Local/EdgeEngine/build/Libs/D3D12MemoryAllocator/src/CMakeFiles/Export/ee588b3a91b74838523c9953fc9affc2/D3D12MemoryAllocatorTargets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/cmake/D3D12MemoryAllocator" TYPE FILE FILES "D:/Dev/Local/EdgeEngine/build/Libs/D3D12MemoryAllocator/src/CMakeFiles/Export/ee588b3a91b74838523c9953fc9affc2/D3D12MemoryAllocatorTargets-debug.cmake")
  endif()
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/cmake/D3D12MemoryAllocator" TYPE FILE FILES "D:/Dev/Local/EdgeEngine/build/Libs/D3D12MemoryAllocator/src/CMakeFiles/Export/ee588b3a91b74838523c9953fc9affc2/D3D12MemoryAllocatorTargets-minsizerel.cmake")
  endif()
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/cmake/D3D12MemoryAllocator" TYPE FILE FILES "D:/Dev/Local/EdgeEngine/build/Libs/D3D12MemoryAllocator/src/CMakeFiles/Export/ee588b3a91b74838523c9953fc9affc2/D3D12MemoryAllocatorTargets-relwithdebinfo.cmake")
  endif()
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/cmake/D3D12MemoryAllocator" TYPE FILE FILES "D:/Dev/Local/EdgeEngine/build/Libs/D3D12MemoryAllocator/src/CMakeFiles/Export/ee588b3a91b74838523c9953fc9affc2/D3D12MemoryAllocatorTargets-release.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "D:/Dev/Local/EdgeEngine/build/Libs/D3D12MemoryAllocator/src/Debug/D3D12MAd.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "D:/Dev/Local/EdgeEngine/build/Libs/D3D12MemoryAllocator/src/Release/D3D12MA.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "D:/Dev/Local/EdgeEngine/build/Libs/D3D12MemoryAllocator/src/MinSizeRel/D3D12MAs.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "D:/Dev/Local/EdgeEngine/build/Libs/D3D12MemoryAllocator/src/RelWithDebInfo/D3D12MArd.lib")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include" TYPE FILE FILES "D:/Dev/Local/EdgeEngine/Libs/D3D12MemoryAllocator/include/D3D12MemAlloc.h")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "D:/Dev/Local/EdgeEngine/build/Libs/D3D12MemoryAllocator/src/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
