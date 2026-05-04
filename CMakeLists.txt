cmake_minimum_required(VERSION 3.25)
project(redasm-processors C)

if(NOT TARGET redasm::redasm)
    find_package(redasm REQUIRED)
endif()

add_subdirectory(capstonebundle)
add_subdirectory(x86)
add_subdirectory(mips)
