cmake_minimum_required (VERSION 3.16)

project (Utils)

add_compile_options(/MP)
add_compile_options(/std:c++17)

set (Lib_headers
    "Libs/stb/stb_image.h"
    "Libs/stb/stb_image_resize.h"
    "Libs/stb/stb_image_write.h"
)

set (Headers
    "Source/Log.h"
    "Source/utils.h"
    "Source/Multithreading.h"
    "Source/SystemInfo.h"
    "Source/Image.h"
    "Source/Timer.h"
)

set (Source
    "Source/Log.cpp"
    "Source/utils.cpp"
    "Source/Multithreading.cpp"
    "Source/SystemInfo.cpp"
    "Source/Image.cpp"
    "Source/Timer.cpp"
)

source_group("Libs"   FILES ${Lib_headers})
set_source_files_properties(${Lib_headers} PROPERTIES VS_TOOL_OVERRIDE "Text")
add_definitions(-DNOMINMAX)

# set ouput directory
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Lib)
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set( CMAKE_LIBRARY_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/Bin/${OUTPUTCONFIG} )
    set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/Lib/${OUTPUTCONFIG} )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

add_library(${PROJECT_NAME} STATIC ${Headers} ${Source} ${Lib_headers})