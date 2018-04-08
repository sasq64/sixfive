cmake_minimum_required(VERSION 2.8.4)
project(sixfive)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

set(APONE ../chipmachine/apone/mods)

set(SOURCE_FILES
	emulator.cpp
	main.cpp
	assembler.cpp
	parser.cpp
	tests.cpp
	${APONE}/coreutils/file.cpp
	${APONE}/coreutils/utils.cpp
	${APONE}/bbsutils/console.cpp
	${APONE}/bbsutils/ansiconsole.cpp
	${APONE}/bbsutils/localconsole.cpp
	${APONE}/bbsutils/editor.cpp
)

add_subdirectory(zyan-disassembler-engine)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wno-error=deprecated-declarations -O2 -fno-omit-frame-pointer -march=native -std=c++17")

include_directories(. /usr/local/include ${APONE})

add_definitions(-DUSE_APONE -DSIMPLE_LOG)

FILE(GLOB_RECURSE IncFiles "*.h")
add_custom_target(headers SOURCES ${IncFiles})

add_executable(sixfive ${SOURCE_FILES})
target_link_libraries(sixfive Zydis benchmark)
if(UNIX)
	target_link_libraries(sixfive pthread)
endif()
