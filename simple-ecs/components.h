#pragma once

#include "simple-ecs/entity.h"


using Component = IDType;


template<typename... T>
struct Components {
    using Type = Components<T...>;
};

template<typename... T>
struct Components<Components<T...>> {
    using Type = Components<T...>;
};

template<typename... T, typename... U>
struct Components<Components<T...>, Components<U...>> {
    using Type = Components<T..., U...>;
};

template<typename... T, typename... U, typename... V>
struct Components<Components<T...>, Components<U...>, V...> {
    using Type = Components<typename Components<T..., U...>::Type, V...>::Type;
};

template<>
struct Components<> {
    using Type = Components<>;
};

template<typename Component>
struct Updated {};


template<typename Component>
struct RemoveTag {
    using Type = Component;
};

template<typename Component>
struct RemoveTag<Updated<Component>> {
    using Type = Component;
};

template<typename Component>
using RemoveTag_t = typename RemoveTag<Component>::Type;
