#pragma once

#include "simple-ecs/entity.h"
#include "tools/crc_ct.h"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <cassert>
#include <string_view>
#include <vector>


#if __clang__
#define ECS_OFFSET_LEFT 29
#define ECS_OFFSET_RIGHT 1
#define ECS_FORCEINLINE __forceinline
#elif _MSC_VER
#define __PRETTY_FUNCTION__ __FUNCSIG__ // NOLINT //-V2573
#define ECS_OFFSET_LEFT 79
#define ECS_OFFSET_RIGHT 7
#define ECS_FORCEINLINE __forceinline
#elif defined(__GNUC__) || defined(__GNUG__)
#define ECS_OFFSET_LEFT 44
#define ECS_OFFSET_RIGHT 50
#define ECS_FORCEINLINE __attribute__((always_inline))
#endif

#define ECS_CONCAT_IMPL(x, y) x##y
#define ECS_MACRO_CONCAT(x, y) ECS_CONCAT_IMPL(x, y)
#define ECS_DEFINE_UNIQUE(name) ECS_MACRO_CONCAT(name, __COUNTER__)

#if _DEBUG
#define ECS_DEBUG_ONLY(...) __VA_ARGS__
#define ECS_RELEASE_ONLY(...)
#else
#define ECS_DEBUG_ONLY(...)
#define ECS_RELEASE_ONLY(...) __VA_ARGS__
#endif

#if ECS_FINAL
#define ECS_FINAL_ONLY(...) __VA_ARGS__
#define ECS_NOT_FINAL_ONLY(...)
#else
#define ECS_FINAL_ONLY(...)
#define ECS_NOT_FINAL_ONLY(...) __VA_ARGS__
#endif

#define ECS_CASE(...) __VA_ARGS__
#define ECS_DEBUG_RELEASE_SWITCH(x, y) ECS_DEBUG_ONLY(x) ECS_RELEASE_ONLY(y)
#define ECS_FINAL_SWITCH(x, y) ECS_FINAL_ONLY(x) ECS_NOT_FINAL_ONLY(y)


#define ECS_ASSERT(EXPR, MSG)                                                         \
    {                                                                                 \
        assert((EXPR) || ([]() {                                                      \
                   spdlog::critical("TYPE: {}, MSG: {}\n\n", name<Component>(), MSG); \
                   return false;                                                      \
               }()));                                                                 \
    }

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

template<auto N>
struct string_literal {
    constexpr string_literal(const char (&str)[N]) : size(N) { // NOLINT
        std::copy_n(str, N, value);
    }
    char        value[N]{}; // NOLINT
    std::size_t size;
};

template<string_literal T>
consteval auto operator"" _crc() {
    return crc32::compute(T.value);
}


template<typename T>
consteval std::string_view name() {
    constexpr std::string_view name(__PRETTY_FUNCTION__);
    return name;
}

template<typename... T>
consteval std::string_view name(T&&...) { // NOLINT
    constexpr std::string_view name(__PRETTY_FUNCTION__);
    return name;
}

template<typename T>
consteval std::string_view svName() {
    auto fn = name<T>();
    return {fn.begin() + ECS_OFFSET_LEFT, fn.end() - ECS_OFFSET_RIGHT};
}

template<typename T>
inline constinit std::string_view SV_NAME = svName<T>(); // NOLINT

template<typename T>
constexpr std::string sName() {
    auto fn = name<T>();
    return {fn.begin() + ECS_OFFSET_LEFT, fn.end() - ECS_OFFSET_RIGHT};
}

template<typename T>
std::string S_NAME = sName<T>(); // NOLINT

template<typename T>
consteval IDType id() {
    return crc32::compute(name<T>());
};

template<typename T>
constinit IDType ID = id<T>(); // NOLINT

template<typename T>
constexpr std::string serializeId() {
    auto id = ID<T>;
    return {std::launder(reinterpret_cast<const char* const>(&id)), sizeof(id)};
}

// operators
template<typename T>
inline const std::vector<T>& operator|(const std::vector<T>& lhs, const std::vector<T>& rhs) {
    if (lhs.empty()) {
        return rhs; // NOLINT
    }

    if (rhs.empty()) {
        return lhs; // NOLINT
    }

    static std::vector<T> result;
    result.resize(lhs.size());

    auto it = std::set_union(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend(), result.begin());
    result.erase(it, result.end());
    return result;
}

inline const std::vector<Entity> EMPTY_ARRAY;

template<typename T>
inline const std::vector<T>& operator&(const std::vector<T>& lhs, const std::vector<T>& rhs) {
    if (lhs.empty() || rhs.empty()) {
        return EMPTY_ARRAY;
    }

    static std::vector<T> result;
    result.resize(lhs.size());

    auto it = std::set_intersection(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), result.begin());
    result.erase(it, result.end());
    return result;
}

template<typename T>
inline const std::vector<T>& operator-(const std::vector<T>& lhs, const std::vector<T>& rhs) {
    if (lhs.empty()) {
        return EMPTY_ARRAY;
    }

    if (rhs.empty()) {
        return lhs; // NOLINT
    }

    static std::vector<T> result;
    result.resize(lhs.size());

    auto it = std::set_difference(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), result.begin());
    result.erase(it, result.end());
    return result;
}

template<typename T>
inline const std::vector<T>& operator+(const std::vector<T>& lhs, const std::vector<T>& rhs) { // append only uniq
    if (lhs.empty()) {
        return rhs; // NOLINT
    }

    if (rhs.empty()) {
        return lhs; // NOLINT
    }

    static std::vector<T> result;
    result = lhs;

    // we have to use an ordered vector for the operator-()
    std::vector<T> temp = lhs;
    std::sort(temp.begin(), temp.end());

    auto uniq_to_append = rhs - temp;

    result.insert(result.end(), uniq_to_append.begin(), uniq_to_append.end());
    return result;
}

// compile time enum with string values
#define ECS_MAKE_ENUM(enum_name, ...)                                                                            \
    enum class enum_name { __VA_ARGS__, ENUM_SIZE };                                                             \
    inline constinit const std::size_t ENUM_##enum_name##_SIZE = static_cast<std::size_t>(enum_name::ENUM_SIZE); \
    template<enum_name Value>                                                                                    \
    inline consteval std::string_view name() {                                                                   \
        static_assert(Value < enum_name::ENUM_SIZE);                                                             \
        std::string_view str   = #__VA_ARGS__;                                                                   \
        std::string_view delim = ", ";                                                                           \
        std::size_t      index = 0;                                                                              \
        std::size_t      pos   = 0;                                                                              \
        while (std::string::npos != pos) {                                                                       \
            std::size_t begin  = pos;                                                                            \
            pos                = str.find(delim, pos) + delim.size();                                            \
            std::size_t end    = std::string::npos == pos ? str.size() : pos - delim.size();                     \
            std::size_t length = end - begin;                                                                    \
            if (static_cast<std::size_t>(Value) == index) {                                                      \
                return str.substr(begin, length);                                                                \
            }                                                                                                    \
            index++;                                                                                             \
        }                                                                                                        \
        return "";                                                                                               \
    }
