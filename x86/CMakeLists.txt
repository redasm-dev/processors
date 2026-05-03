cmake_minimum_required(VERSION 3.25)

project(x86 C)

if(NOT TARGET redasm::redasm)
    find_package(redasm REQUIRED)
endif()

include(cmake/Dependencies.cmake)
setup_dependencies()

add_library(${PROJECT_NAME} SHARED)

target_sources(${PROJECT_NAME}
    PRIVATE 
        plugin.c
        lifter.c
        common.c
        registers.c
)

target_link_libraries(${PROJECT_NAME} 
    PRIVATE 
        redasm::redasm
        Zydis
)
