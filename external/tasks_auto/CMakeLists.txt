if(TARGET Tasks::Tasks)
    return()
endif()

message(STATUS "external: downloading target 'Tasks::Tasks'")

include(FetchContent)
FetchContent_Declare(
    tasks_git
    GIT_REPOSITORY https://gitlab.com/p34ch-main/tasks
    GIT_TAG        v1.1.0
)

FetchContent_MakeAvailable(tasks_git)
