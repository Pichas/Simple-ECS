if(TARGET spdlog::spdlog)
    return()
endif()

message(STATUS "external: downloading target 'spdlog::spdlog'")

include(FetchContent)
FetchContent_Declare(
    spdlog_git
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
)

option(SPDLOG_USE_STD_FORMAT "Use std::format instead of fmt library." ON)
option(SPDLOG_NO_EXCEPTIONS "Compile with -fno-exceptions. Call abort() on any spdlog exceptions" ON)

FetchContent_MakeAvailable(spdlog_git)
