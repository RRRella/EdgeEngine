cmake_minimum_required (VERSION 3.16)
set(CMAKE_GENERATOR_PLATFORM x64)

project (Edgine)

add_compile_options(/MP)
add_link_options(/SUBSYSTEM:WINDOWS)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/Bin)
foreach( OUTPUTCONFIG ${CMAKE_CONFIGURATION_TYPES} )
    string( TOUPPER ${OUTPUTCONFIG} OUTPUTCONFIG )
    set( CMAKE_RUNTIME_OUTPUT_DIRECTORY_${OUTPUTCONFIG} ${CMAKE_BINARY_DIR}/Bin/${OUTPUTCONFIG} )
endforeach( OUTPUTCONFIG CMAKE_CONFIGURATION_TYPES )

set (Resource 
    "Data/Resources/Edgine.rc"
    "Data/Resources/resource.h"
    "Data/Icons/Edgine.ico"
)

set (Config
    "Data/EngineSettings.ini"
)

set (Shaders
    "Source/Shaders/hello-triangle.hlsl"
    "Source/Shaders/hello-cube.hlsl"
    "Source/Shaders/FullscreenTriangle.hlsl"
)

set (HeaderEdgine 
    "Source/Application/Platform.h"
    "Source/Application/Window.h"
    "Source/Application/Settings.h"
    "Source/Application/Types.h"
    "Source/Application/Engine.h"
    "Source/Application/Events.h"
    "Source/Application/Mesh.h"
    "Source/Application/Geometry.h"
)

set (SourceEdgine 
    "Source/Application/Platform.cpp"
    "Source/Application/Window.cpp"
    "Source/Application/Main.cpp"
    "Source/Application/Engine_Main.cpp"
    "Source/Application/Engine_Render.cpp"
    "Source/Application/Engine_Update.cpp"
    "Source/Application/Engine_WindowEvents.cpp"
    "Source/Application/Events.cpp"
    "Source/Application/Mesh.cpp"
    "Source/Application/Geometry.cpp"
)


add_subdirectory(Source/Utils)
add_subdirectory(Libs/D3D12MemoryAllocator)
add_subdirectory(Source/Renderer)

source_group("Config"   FILES ${Config})
source_group("Resource" FILES ${Resource})
source_group("Icons"    FILES ${Icons})
source_group("Shaders"  FILES ${Shaders})

set_source_files_properties(${Config} PROPERTIES VS_TOOL_OVERRIDE "Text")
set_source_files_properties(${Shaders} PROPERTIES VS_TOOL_OVERRIDE "Text")
set_source_files_properties(Data/Resources/Edgine.rc PROPERTIES VS_TOOL_OVERRIDE "Resource compiler")
set_source_files_properties(Data/Icons/Edgine.ico  PROPERTIES VS_TOOL_OVERRIDE "Image")

add_executable( ${PROJECT_NAME} ${HeaderEdgine} ${SourceEdgine} ${Config} ${Resource} ${Shaders} )

set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/Bin/ )

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Data ${CMAKE_BINARY_DIR}/Bin/Data)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Source/Shaders ${CMAKE_BINARY_DIR}/Bin/Shaders)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Data $<TARGET_FILE_DIR:${PROJECT_NAME}>/Data)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Source/Shaders $<TARGET_FILE_DIR:${PROJECT_NAME}>/Shaders)

# TODO: the above commands must run even if the project is up to date which is not the case now.
#       if Data/EngineSettings.ini is updated, the copy in the target destination of the commands above
#       is not updated.

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${Includes})


target_link_libraries(${PROJECT_NAME} PRIVATE Renderer Utils)
