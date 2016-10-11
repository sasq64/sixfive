cmake_minimum_required(VERSION 2.8.4)
project(sixfive)

set(SOURCE_FILES
main.cpp
emulator.cpp
assembler.cpp
parser.cpp
../apone/mods/coreutils/file.cpp
)

add_subdirectory(zyan-disassembler-engine)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O2 -march=native -std=c++14")
include_directories(. /usr/local/include ../apone/mods)

add_definitions(-DUSE_APONE)

FILE(GLOB_RECURSE IncFiles "*.h")
add_custom_target(headers SOURCES ${IncFiles})

add_executable(sixfive ${SOURCE_FILES})
target_link_libraries(sixfive Zydis benchmark)
