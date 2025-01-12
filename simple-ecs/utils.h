#pragma once

#include "simple-ecs/entity.h"
#include "simple-ecs/tools/profiler.h" // IWYU pragma: export
#include <spdlog/spdlog.h>
#include <ct/names.h>
#include <tmp_buffer/tmp_buffer.h>

#include <algorithm>
#include <cassert>
#include <span>
#include <vector>


// clang-format off
#if ECS_FINAL
#   if __clang__
#       define ECS_FORCEINLINE __attribute__((always_inline)) inline
#   elif _MSC_VER
#       define __PRETTY_FUNCTION__ __FUNCSIG__ // NOLINT //-V2573
#       define ECS_FORCEINLINE __forceinline
#   elif defined(__GNUC__) || defined(__GNUG__)
#       define ECS_FORCEINLINE __attribute__((always_inline))
#   endif
#else // ECS_FINAL
#   define ECS_FORCEINLINE
#endif // ECS_FINAL

#define ECS_CONCAT_IMPL(x, y) x##y
#define ECS_MACRO_CONCAT(x, y) ECS_CONCAT_IMPL(x, y)
#define ECS_DEFINE_UNIQUE(name) ECS_MACRO_CONCAT(name, __COUNTER__)

#ifdef _DEBUG
#   define ECS_DEBUG_ONLY(...) __VA_ARGS__
#   define ECS_RELEASE_ONLY(...)
#else
#   define ECS_DEBUG_ONLY(...)
#   define ECS_RELEASE_ONLY(...) __VA_ARGS__
#endif

#if ECS_FINAL
#   define ECS_FINAL_ONLY(...) __VA_ARGS__
#   define ECS_NOT_FINAL_ONLY(...)
#else
#   define ECS_FINAL_ONLY(...)
#   define ECS_NOT_FINAL_ONLY(...) __VA_ARGS__
#endif

#define ECS_CASE(...) __VA_ARGS__
#define ECS_DEBUG_RELEASE_SWITCH(x, y) ECS_DEBUG_ONLY(x) ECS_RELEASE_ONLY(y)
#define ECS_FINAL_SWITCH(x, y) ECS_FINAL_ONLY(x) ECS_NOT_FINAL_ONLY(y)
// clang-format on


#define ECS_ASSERT(EXPR, MSG)                                                           \
    do {                                                                                \
        assert((EXPR) || ([]() {                                                        \
                   spdlog::critical("TYPE: {}, MSG: {}\n\n", ct::name<Component>, MSG); \
                   return false;                                                        \
               }()));                                                                   \
    } while (0)

// we cannot use static_assert(false), but we can use this function instead
void compiletimeFail(const char*);

struct NoCopy {
    NoCopy()                         = default;
    NoCopy(const NoCopy&)            = delete;
    NoCopy& operator=(const NoCopy&) = delete;
    NoCopy(NoCopy&&)                 = default;
    NoCopy& operator=(NoCopy&&)      = default;
};

struct NoCopyNoMove {
    NoCopyNoMove()                               = default;
    NoCopyNoMove(const NoCopyNoMove&)            = delete;
    NoCopyNoMove& operator=(const NoCopyNoMove&) = delete;
    NoCopyNoMove(NoCopyNoMove&&)                 = delete;
    NoCopyNoMove& operator=(NoCopyNoMove&&)      = delete;
};

template<typename Target>
concept EcsTarget = std::disjunction_v<std::is_same<Target, Entity>, std::is_same<Target, std::span<const Entity>>>;

using TmpBufferVector = decltype(TMP_GET(std::vector<Entity>));
using EntitiesWrapper = std::reference_wrapper<const std::vector<Entity>>;

template<typename T>
constexpr std::string serializeId() {
    auto id = ct::ID<T>;
    return {std::launder(reinterpret_cast<const char* const>(&id)), sizeof(id)}; //-V206
}


// operators
template<typename T>
ECS_FORCEINLINE static TmpBufferVector operator|(const std::vector<T>& lhs, const std::vector<T>& rhs) {
    ECS_PROFILER(ZoneScoped);

    if (lhs.empty()) {
        auto result = TMP_GET(std::vector<Entity>);
        result->insert(result->end(), rhs.begin(), rhs.end());
        return result;
    }

    if (rhs.empty()) {
        auto result = TMP_GET(std::vector<Entity>);
        result->insert(result->end(), lhs.begin(), lhs.end());
        return result;
    }

    auto result = TMP_GET(std::vector<Entity>);
    result->reserve(lhs.size() + rhs.size());

    std::ranges::set_union(lhs, rhs, std::back_inserter(*result));
    return result;
}

template<typename T>
ECS_FORCEINLINE static TmpBufferVector operator|(const std::vector<T>& lhs, TmpBufferVector rhs) {
    if (lhs.empty()) {
        return rhs;
    }

    return lhs | *rhs;
}


template<typename T>
ECS_FORCEINLINE static TmpBufferVector operator&(const std::vector<T>& lhs, const std::vector<T>& rhs) {
    ECS_PROFILER(ZoneScoped);

    if (lhs.empty() || rhs.empty()) {
        return TMP_GET(std::vector<Entity>);
    }

    auto result = TMP_GET(std::vector<Entity>);
    result->reserve(lhs.size());

    std::ranges::set_intersection(lhs, rhs, std::back_inserter(*result));
    return result;
}


template<typename T>
ECS_FORCEINLINE static TmpBufferVector operator&(const std::vector<T>& lhs, TmpBufferVector rhs) {
    return lhs & *rhs;
}

ECS_FORCEINLINE static TmpBufferVector operator-(TmpBufferVector lhs, TmpBufferVector rhs) { // NOLINT
    ECS_PROFILER(ZoneScoped);

    if (lhs->empty()) {
        return TMP_GET(std::vector<Entity>);
    }

    if (rhs->empty()) {
        return lhs;
    }

    auto result = TMP_GET(std::vector<Entity>);
    result->reserve(lhs->size());

    std::ranges::set_difference(*lhs, *rhs, std::back_inserter(*result));
    return result;
}
