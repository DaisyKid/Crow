cmake_minimum_required(VERSION 3.15)
project(crow_main)

include(${CMAKE_SOURCE_DIR}/cmake/compiler_options.cmake)

set(INC_DIR /usr/include)
set(LINK_DIR /usr/lib/x86_64-linux-gnu)
include_directories(${INC_DIR})
link_directories(${LINK_DIR})

add_executable(stage_instrument stage_instrument.cpp)
add_warnings_optimizations(stage_instrument)
target_link_libraries(stage_instrument PUBLIC Crow::Crow tinyxml2)
