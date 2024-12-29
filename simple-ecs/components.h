#pragma once

#include "simple-ecs/entity.h"
#include <type_traits>


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


template<typename... Component>
struct Archetype : Component... {
    using Components = Components<Component...>;
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

template<typename...>
struct RemoveTags;

template<typename... Component>
struct RemoveTags<Components<Component...>> : RemoveTag<Component>... {
    using Type = typename Components<RemoveTag_t<Component>...>::Type;
};

template<typename Component>
using RemoveTags_t = typename RemoveTags<typename Component::Type>::Type;


template<typename...>
struct RemoveEmpty;

template<typename Component>
requires(!std::is_empty_v<Component>)
struct RemoveEmpty<Component> {
    using Type = Components<Component>;
};


template<typename Component>
requires(std::is_empty_v<Component>)
struct RemoveEmpty<Component> {
    using Type = Components<>;
};

template<typename... Component>
struct RemoveEmpty<Components<Component...>> : RemoveEmpty<Component>... {
    using Type = typename Components<typename RemoveEmpty<Component>::Type...>::Type;
};

template<typename Component>
using RemoveEmpty_t = typename RemoveEmpty<typename Component::Type>::Type;
