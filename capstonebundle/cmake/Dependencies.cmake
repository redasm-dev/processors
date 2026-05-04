include(cmake/CPM.cmake)

function(setup_dependencies)
    CPMAddPackage(
        NAME capstone
        GIT_TAG "6.0.0-Alpha7"
        GITHUB_REPOSITORY "capstone-engine/capstone"

        OPTIONS 
            "CAPSTONE_ARCHITECTURE_DEFAULT OFF"
            "CAPSTONE_ARM_SUPPORT ON"
            "CAPSTONE_AARCH64_SUPPORT ON"
    )
endfunction()
