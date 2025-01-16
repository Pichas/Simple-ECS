option(SPDLOG_NO_EXCEPTIONS "Compile with -fno-exceptions. Call abort() on any spdlog exceptions" ON)
option(SPDLOG_USE_STD_FORMAT "Use std::format instead of fmt library." ON)


AddExternalPackage(
    spdlog
        v1.14.1
        https://github.com/gabime/spdlog.git
        NO
        NO
)