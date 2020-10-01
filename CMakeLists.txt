cmake_minimum_required(VERSION 3.13.0)
project(igxrt)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

file(GLOB_RECURSE hpp "include/*.hpp")
file(GLOB_RECURSE cpp "src/*.cpp")

set(enableIgxTest FALSE FORCE CACHE BOOL "Enable IGX test")
set(enableIgxRtTest TRUE CACHE BOOL "Enable igx rt test")
add_subdirectory(igx)

# Setup test data

file(GLOB_RECURSE shaders "res/shaders/*.comp" "res/shaders/*.vert" "res/shaders/*.frag")
file(GLOB_RECURSE shaderIncludes "res/shaders/*.glsl")
file(GLOB_RECURSE shaderBinaries "res/shaders/*.spv")

# Link library

add_library(
	rtigx STATIC
	${hpp}
	${cpp}
	${shaders}
	${shaderIncludes}
	${shaderBinaries}
	CMakeLists.txt
)

target_include_directories(rtigx PRIVATE include)
target_include_directories(rtigx PRIVATE igx/include)
target_include_directories(rtigx PRIVATE igx/igxi-tool/igxi/ignis/include)
target_include_directories(rtigx PRIVATE igx/igxi-tool/igxi/ignis/core2/include)
target_link_libraries(rtigx PRIVATE igx)

if(MSVC)
    target_compile_options(rtigx PRIVATE /W4 /WX /MD /MP /wd26812 /wd4201 /EHsc /GR)
else()
    target_compile_options(rtigx PRIVATE -Wall -Wpedantic -Wextra -Werror)
endif()

source_group("Headers" FILES ${hpp})
source_group("Source" FILES ${cpp})
source_group("Shaders" FILES ${shaders})
source_group("Shader includes" FILES ${shaderIncludes})
source_group("Shader binaries" FILES ${shaderBinaries})

add_virtual_files(
	DIRECTORY
		${CMAKE_CURRENT_SOURCE_DIR}/res/shaders
	NAME
		shaders
	FILES
		${shaderBinaries}
)


if(enableIgxRtTest)

	file(GLOB_RECURSE testSrc "test/*.cpp")
	file(GLOB_RECURSE testInc "test/*.hpp")

	add_executable(rtigx_test ${testSrc} ${testInc})
	target_include_directories(rtigx_test PRIVATE include)
	target_include_directories(rtigx_test PRIVATE igx/include)
	target_include_directories(rtigx_test PRIVATE igx/igxi-tool/igxi/ignis/include)
	target_include_directories(rtigx_test PRIVATE igx/igxi-tool/igxi/ignis/core2/include)

	if(MSVC)
	    target_compile_options(rtigx_test PRIVATE /W4 /WX /MD /MP /wd26812 /wd4201 /EHsc /GR)
	else()
	    target_compile_options(rtigx_test PRIVATE -Wall -Wpedantic -Wextra -Werror)
	endif()

	target_link_libraries(rtigx_test PRIVATE rtigx)

	set_property(TARGET rtigx_test PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/res")

	if(doShaderRecreate)

		add_custom_command(
			TARGET rtigx_test
			PRE_BUILD COMMAND
			ARGS
			"${CMAKE_CURRENT_SOURCE_DIR}/res/shaders/compile.sh"
			"$<$<CONFIG:debug>:-d>"
			${shaders}
		)

	endif()


	add_virtual_files(
		DIRECTORY
			${CMAKE_CURRENT_SOURCE_DIR}/res/textures
		NAME
			textures
		FILES
			${CMAKE_CURRENT_SOURCE_DIR}/res/textures/qwantani_4k.hdr
	)

	configure_icon(rtigx_test "${CMAKE_CURRENT_SOURCE_DIR}/igx/res/icon.ico")
	configure_virtual_files(rtigx_test)

endif()