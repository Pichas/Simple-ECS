#pragma once

#include "simple-ecs/utils.h"

// WARN: we erase types here, so now you are responsible to match types
struct VoidCallback {
    virtual ~VoidCallback() = default;

    template<typename... Args>
    void operator()(Args&&... args) const {
        assert(m_ptr);
        auto& f = *static_cast<std::function<void(Args...)>*>(m_ptr);
        std::invoke(f, std::forward<Args>(args)...);
    }

    template<typename R, typename... Args>
    R invoke(Args&&... args) const {
        assert(m_ptr);
        auto& f = *static_cast<std::function<R(Args...)>*>(m_ptr);
        if constexpr (std::is_void_v<R>) {
            std::invoke(f, std::forward<Args>(args)...);
        } else {
            return std::invoke(f, std::forward<Args>(args)...);
        }
    }

protected:
    VoidCallback() = default;

protected:
    void* m_ptr = nullptr;
};

namespace detail
{
template<typename...>
struct VoidCallbackHandler;

template<typename R, typename Func, typename... Args>
requires(std::is_invocable_r_v<R, Func, Args...>)
struct VoidCallbackHandler<R, Func, Args...> final : VoidCallback, NoCopyNoMove {
    VoidCallbackHandler(Func&& f) : m_memory(std::forward<Func>(f)) { m_ptr = &m_memory; }

private:
    std::function<R(Args...)> m_memory;
};

template<typename R, typename... Args>
struct VoidCallbackHandler<R, Args...> final : VoidCallback, NoCopyNoMove {
    VoidCallbackHandler(const std::function<R(Args...)>& f) : m_memory(f) { m_ptr = &m_memory; }
    VoidCallbackHandler(R (*f)(Args...))
    requires(std::is_invocable_r_v<R, decltype(f), Args...>)
      : m_memory(f) {
        m_ptr = &m_memory;
    }

private:
    std::function<R(Args...)> m_memory;
};
} // namespace detail

// you need to explicitly specify the arguments types here
// auto cb = voidCallbackBuilder<int, float>([](int x, float y) { });
template<typename... Args, typename Func, typename R = std::invoke_result_t<Func, Args...>>
requires(std::is_invocable_r_v<R, Func, Args...>)
auto voidCallbackBuilder(Func&& f) {
    return std::make_unique<detail::VoidCallbackHandler<R, Func, Args...>>(std::forward<Func>(f));
}

// std::function<void(int, float)> f = [](int x, float y) { };
// auto cb = voidCallbackBuilder(f);
template<typename R, typename... Args>
auto voidCallbackBuilder(const std::function<R(Args...)>& f) {
    return std::make_unique<detail::VoidCallbackHandler<R, Args...>>(f);
}

// auto cb = voidCallbackBuilder(+[](int x, float y) { });
template<typename R, typename... Args>
auto voidCallbackBuilder(R (*f)(Args...))
requires(std::is_invocable_r_v<R, decltype(f), Args...>)
{
    return std::make_unique<detail::VoidCallbackHandler<R, Args...>>(f);
}

//
// how to call it
// (*cb)(42, 3.14f); <- don't forget to put right types
//
// or with return value
// auto result = cb->invoke<int>(42, 3.14f); <- don't forget to put right types
//