#pragma once

#include "simple-ecs/components.h"
#include "simple-ecs/utils.h"
#include "simple-ecs/world.h"

#include <array>
#include <tmp_buffer/tmp_buffer.h>


template<typename... C>
struct Require {
    using Type = Components<C...>;
};

template<typename... C>
struct Require<Archetype<C...>> {
    using Type = Components<C...>;
};


template<typename... C>
struct Exclude {
    using Type = Components<C...>;
};

template<typename... C>
struct Exclude<Archetype<C...>> {
    using Type = Components<C...>;
};


template<typename...>
struct Filter;

template<typename... R, typename... E>
struct Filter<Require<R...>, Exclude<E...>> : Require<R...>, Exclude<E...> {};

template<typename... R>
struct Filter<Require<R...>> : Require<R...>, Exclude<> {};

template<typename... E>
struct Filter<Exclude<E...>> : Require<>, Exclude<E...> {};

template<>
struct Filter<> : Require<>, Exclude<> {};

struct RunEveryFrame final : Filter<> {};


template<typename...>
struct CheckTypes;

template<typename Component, typename... Types>
struct CheckTypes<Component, Components<Types...>> : std::disjunction<std::is_same<Component, RemoveTag_t<Types>>...> {
};

template<typename...>
struct CheckTypesAllOf;

template<typename... Component, typename... Types>
struct CheckTypesAllOf<Components<Component...>, Components<Types...>>
  : std::conjunction<CheckTypes<Component, Components<Types...>>...> {};

template<typename...>
struct CheckTypesAnyOf;

template<typename... Component, typename... Types>
struct CheckTypesAnyOf<Components<Component...>, Components<Types...>>
  : std::disjunction<CheckTypes<Component, Components<Types...>>...> {};

template<typename Component, typename... Types>
inline constexpr bool ALL_OF = CheckTypesAllOf<Component, Types...>::value;


template<typename Component, typename... Types>
inline constexpr bool ANY_OF = !CheckTypesAnyOf<Component, Types...>::value;


template<typename... T>
struct AND {};

template<typename... T>
struct OR {};

template<typename... T>
struct FilteredEntities;

template<typename... Component>
requires(sizeof...(Component) > 1)
struct FilteredEntities<AND<Components<Component...>>> {
    ECS_FORCEINLINE static TmpBufferVector ents(const World& world) {
        ECS_PROFILER(ZoneScoped);

        std::array<EntitiesWrapper, sizeof...(Component)> ents_by_comps{world.entities<Component>()...};
        std::ranges::sort(
          ents_by_comps, std::less<>{}, [](const EntitiesWrapper& v) noexcept { return v.get().size(); });

        return [&]<size_t... Is>(std::index_sequence<Is...>) {
            return (ents_by_comps[Is].get() & ...);
        }(std::make_index_sequence<sizeof...(Component)>{});
    }
};

template<typename Component>
struct FilteredEntities<AND<Components<Component>>> {
    ECS_FORCEINLINE static TmpBufferVector ents(const World& world) {
        ECS_PROFILER(ZoneScoped);

        const auto& entities = world.entities<Component>();
        auto        result   = TMP_GET(std::vector<Entity>);
        result->insert(result->end(), entities.cbegin(), entities.cend());

        return result;
    }
};

template<>
struct FilteredEntities<AND<Components<>>> {
    FilteredEntities(const World& /*unused*/) {}
    ECS_FORCEINLINE static TmpBufferVector ents(const World& /*world*/) { return TMP_GET(std::vector<Entity>); }
};


template<typename... Component>
requires(sizeof...(Component) > 1)
struct FilteredEntities<OR<Components<Component...>>> {
    ECS_FORCEINLINE static TmpBufferVector ents(const World& world) {
        ECS_PROFILER(ZoneScoped);

        std::array<EntitiesWrapper, sizeof...(Component)> ents_by_comps{world.entities<Component>()...};
        std::ranges::sort(
          ents_by_comps, std::less<>{}, [](const EntitiesWrapper& v) noexcept { return v.get().size(); });

        return [&]<size_t... Is>(std::index_sequence<Is...>) {
            return (ents_by_comps[Is].get() | ...);
        }(std::make_index_sequence<sizeof...(Component)>{});
    }
};

template<typename Component>
struct FilteredEntities<OR<Components<Component>>> {
    ECS_FORCEINLINE static TmpBufferVector ents(const World& world) {
        ECS_PROFILER(ZoneScoped);

        const auto& entities = world.entities<Component>();
        auto        result   = TMP_GET(std::vector<Entity>);
        result->insert(result->end(), entities.cbegin(), entities.cend());

        return result;
    }
};


template<>
struct FilteredEntities<OR<Components<>>> {
    ECS_FORCEINLINE static TmpBufferVector ents(const World& /*world*/) { return TMP_GET(std::vector<Entity>); }
};
