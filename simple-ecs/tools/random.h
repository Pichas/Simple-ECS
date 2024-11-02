#pragma once

#include <random>

template<typename T>
requires std::is_arithmetic_v<T>
inline T dice(T min, T max) {
    static std::mt19937 mt{std::random_device{}()};

    using Type = std::conditional_t<std::is_integral_v<T>, //
                                    std::uniform_int_distribution<T>,
                                    std::uniform_real_distribution<T>>;

    return Type(min, max)(mt);
}
