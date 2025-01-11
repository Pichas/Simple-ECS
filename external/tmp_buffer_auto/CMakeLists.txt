if(TARGET TempBuffer::TempBuffer)
    return()
endif()

message(STATUS "external: downloading target 'TempBuffer::TempBuffer'")

include(FetchContent)
FetchContent_Declare(
    buffers_git
    GIT_REPOSITORY https://gitlab.com/p34ch-main/temp-buffer.git
    GIT_TAG        v1.0.2
)

FetchContent_MakeAvailable(buffers_git)
