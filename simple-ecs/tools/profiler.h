#pragma once

#if __has_include(<tracy/Tracy.hpp>)
#include <tracy/Tracy.hpp>

#define ECS_PROFILER(...) __VA_ARGS__
#define ECS_NO_PROFILER(...)
#else //__has_include(<tracy/Tracy.hpp>)
#define ECS_PROFILER(...)
#define ECS_NO_PROFILER(...) __VA_ARGS__
#endif //__has_include(<tracy/Tracy.hpp>)