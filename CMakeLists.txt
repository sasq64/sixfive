cmake_minimum_required(VERSION 2.8.4)
project(sixfive)

set(SOURCE_FILES
main.cpp
emulator.cpp
assembler.cpp
parser.cpp
#../apone/mods/coreutils/log.cpp
#../apone/mods/coreutils/format.cpp
#../apone/mods/coreutils/utils.cpp
../apone/mods/coreutils/file.cpp
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -std=c++14")
# SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} --Wl,-map,sixfive.map")
include_directories(. /usr/local/include ../apone/mods)

add_definitions(-DUSE_APONE)

FILE(GLOB_RECURSE IncFiles "*.h")
add_custom_target(headers SOURCES ${IncFiles})


add_executable(sixfive ${SOURCE_FILES})

# set_target_properties(sixfive PROPERTIES LINK_FLAGS "--Wl,-map,sixfive.map")

