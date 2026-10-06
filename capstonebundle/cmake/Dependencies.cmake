function(setup_dependencies)
    redasm_add_dependency(
        NAME capstone
        VERSION 6
        GIT_TAG "6.0.0-Alpha10"
        GITHUB_REPOSITORY "capstone-engine/capstone"
        GIT_SHALLOW ON

        OPTIONS 
            "CAPSTONE_ARCHITECTURE_DEFAULT OFF"
            "CAPSTONE_ARM_SUPPORT          ON"
            "CAPSTONE_AARCH64_SUPPORT      ON"
            "CAPSTONE_MOS65XX_SUPPORT      ON"
            "CAPSTONE_XTENSA_SUPPORT       ON"
    )
endfunction()
