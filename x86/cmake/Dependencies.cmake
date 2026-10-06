function(setup_dependencies)
    redasm_add_dependency(
        NAME zydis
        VERSION "4.1.1"
        GITHUB_REPOSITORY "zyantific/zydis"
        GIT_SHALLOW ON
        EXCLUDE_FROM_ALL YES

        OPTIONS 
            "CMAKE_POSITION_INDEPENDENT_CODE ON"
            "ZYDIS_BUILD_TOOLS OFF"
            "ZYDIS_BUILD_EXAMPLES OFF"
    )
endfunction()
