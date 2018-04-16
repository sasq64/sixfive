cmake_minimum_required(VERSION 2.8.4)
project(sixfive)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

set(APONE ../chipmachine/apone/mods)

set(SOURCE_FILES
	main.cpp
	assembler.cpp
	parser.cpp
	tests.cpp
)

include_directories(. /usr/local/include ${APONE})
add_subdirectory(${APONE}/coreutils coreutils)
add_subdirectory(${APONE}/bbsutils bbsutils)

add_subdirectory(zyan-disassembler-engine)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -Wno-error=deprecated-declarations -O2 -fno-omit-frame-pointer -march=native -std=c++17")


add_definitions(-DUSE_APONE)

FILE(GLOB_RECURSE IncFiles "*.h")
add_custom_target(headers SOURCES ${IncFiles})

add_executable(sixfive ${SOURCE_FILES})
target_link_libraries(sixfive coreutils bbsutils Zydis benchmark)
if(UNIX)
	target_link_libraries(sixfive pthread)
endif()

add_executable(c64 c64.cpp)
target_link_libraries(c64 coreutils)
