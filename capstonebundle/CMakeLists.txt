cmake_minimum_required(VERSION 3.20)

project(capstone-bundle C)

if(NOT TARGET redasm::redasm)
    find_package(redasm REQUIRED)
endif()

include(cmake/Dependencies.cmake)
setup_dependencies()

add_library(${PROJECT_NAME} SHARED)

target_sources(${PROJECT_NAME}
    PRIVATE 
        arm/arm32/arm32.c
        arm/arm32/common.c
        arm/arm32/lifter.c
        arm/arm32/thumb.c
        arm/arm64.c
        mips/fallback.c
        mips/mips.c
        mos65xx/mos6502.c
        xtensa/xtensa.c
        capstone.c
        module.c
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(${PROJECT_NAME} 
    PRIVATE 
        redasm::redasm
        capstone
)
