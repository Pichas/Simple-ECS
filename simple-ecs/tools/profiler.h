#pragma once

#if __has_include(<tracy/Tracy.hpp>)
#include <tracy/Tracy.hpp>

#define ECS_PROFILER(...) __VA_ARGS__
#else //__has_include(<tracy/Tracy.hpp>)
#define ECS_PROFILER(...)
#endif //__has_include(<tracy/Tracy.hpp>)