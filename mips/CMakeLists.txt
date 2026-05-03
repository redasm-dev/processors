cmake_minimum_required(VERSION 3.25)

project(mips C)

if(NOT TARGET redasm::redasm)
    find_package(redasm REQUIRED)
endif()

add_library(${PROJECT_NAME} SHARED)

target_sources(${PROJECT_NAME}
    PRIVATE 
        decoder/formats.c
        decoder/instructions.c
        decoder/macros.c
        decoder/registers.c
        plugin.c
        registers.c
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(${PROJECT_NAME} 
    PRIVATE 
        redasm::redasm
)
