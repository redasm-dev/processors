include(cmake/CPM.cmake)

function(setup_dependencies)
    CPMAddPackage(
        NAME zydis
        VERSION "4.1.1"
        GITHUB_REPOSITORY "zyantific/zydis"

        OPTIONS 
            "CMAKE_POSITION_INDEPENDENT_CODE ON"
            "ZYDIS_BUILD_TOOLS OFF"
            "ZYDIS_BUILD_EXAMPLES OFF"
    )
endfunction()
