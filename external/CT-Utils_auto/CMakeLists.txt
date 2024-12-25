if(TARGET CT-Utils::CT-Utils)
    return()
endif()

message(STATUS "external: downloading target 'CT-Utils::CT-Utils'")

include(FetchContent)
FetchContent_Declare(
    ct_git
    GIT_REPOSITORY https://gitlab.com/p34ch-main/ct-utils.git
    GIT_TAG        v1.0.0
)

FetchContent_MakeAvailable(ct_git)
