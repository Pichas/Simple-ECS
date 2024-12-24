#pragma once

#include <random>

namespace detail
{
inline std::mt19937& mt() {
    static std::mt19937 mt{std::random_device{}()};
    return mt;
}
} // namespace detail

template<typename T>
requires std::is_arithmetic_v<T>
static T dice(T min = std::numeric_limits<T>::min(), T max = std::numeric_limits<T>::max()) {
    using Type = std::conditional_t<std::is_integral_v<T>, //
                                    std::uniform_int_distribution<T>,
                                    std::uniform_real_distribution<T>>;

    return Type(min, max)(detail::mt());
}
