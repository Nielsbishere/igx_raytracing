cmake_minimum_required(VERSION 3.13.0)
project(igxrt)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

file(GLOB_RECURSE hpp "include/*.hpp")
file(GLOB_RECURSE cpp "src/*.cpp")

add_subdirectory(igx)

# Setup test data

file(GLOB_RECURSE shaders "res/shaders/*.comp" "res/shaders/*.vert" "res/shaders/*.frag")
file(GLOB_RECURSE shaderBinaries "res/shaders/*.spv")

# Link library

add_executable(
	rtigx
	${hpp}
	${cpp}
	${shaders}
	${shaderBinaries}
	CMakeLists.txt
)

set_property(TARGET rtigx PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/res")

target_include_directories(rtigx PRIVATE include)
target_include_directories(rtigx PRIVATE igx/include)
target_include_directories(rtigx PRIVATE igx/ignis/include)
target_include_directories(rtigx PRIVATE igx/ignis/core2/include)
target_link_libraries(rtigx PRIVATE igx)

source_group("Headers" FILES ${ignisHpp})
source_group("Source" FILES ${ignisCpp})
source_group("Shaders" FILES ${shaders})
source_group("Shader binaries" FILES ${shaderBinaries})

add_custom_command(TARGET rtigx POST_BUILD COMMAND compile.sh ${shaders} WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/res/shaders")

if(MSVC)
    target_compile_options(rtigx PRIVATE /W4 /WX /MD /MP /wd26812 /wd4201 /EHsc /GR)
else()
    target_compile_options(rtigx PRIVATE -Wall -Wpedantic -Wextra -Werror)
endif()