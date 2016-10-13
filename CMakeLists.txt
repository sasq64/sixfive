cmake_minimum_required(VERSION 2.8.4)
project(sixfive)

set(SOURCE_FILES
main.cpp
assembler.cpp
parser.cpp
tests.cpp
../apone/mods/coreutils/file.cpp
)

add_subdirectory(zyan-disassembler-engine)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wno-error=deprecated-declarations -O2 -march=native -std=c++14")
include_directories(. /usr/local/include ../apone/mods)

add_definitions(-DUSE_APONE)

FILE(GLOB_RECURSE IncFiles "*.h")
add_custom_target(headers SOURCES ${IncFiles})

add_executable(sixfive ${SOURCE_FILES})
target_link_libraries(sixfive Zydis benchmark)
if(UNIX)
	target_link_libraries(sixfive pthread)
endif()
