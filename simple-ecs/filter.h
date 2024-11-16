#pragma once

#include "simple-ecs/components.h"
#include "simple-ecs/utils.h"
#include "simple-ecs/world.h"


template<typename... C>
struct Require {
    using Type = Components<C...>;
};


template<typename... C>
struct Exclude {
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


template<typename C, typename... T>
struct CheckTypeInArrayImpl;

template<typename Component, typename... Types>
struct CheckTypeInArrayImpl<Component, Components<Types...>>
  : std::disjunction<std::is_same<Component, RemoveTag_t<Types>>...> {};

template<typename Component, typename... Types>
inline constexpr bool TYPE_REQUESTED = CheckTypeInArrayImpl<Component, Types...>::value;

template<typename Component, typename... Types>
inline constexpr bool TYPE_EXCLUDED = !CheckTypeInArrayImpl<Component, Types...>::value;


using EntitiesWrapper = std::reference_wrapper<const std::vector<Entity>>;

template<typename... T>
struct AND {};

template<typename... T>
struct OR {};

template<typename... T>
struct FilteredEntities;

template<typename... Component>
struct FilteredEntities<AND<Components<Component...>>> {
    FilteredEntities(const World& world) {
        static std::vector<EntitiesWrapper> ents_by_comps{world.entities<Component>()...};
        std::ranges::sort(ents_by_comps, std::less<>{}, [](const EntitiesWrapper& v) { return v.get().size(); });

        entities = ents_by_comps.begin()->get();
        for (auto it = std::next(ents_by_comps.begin()); it != ents_by_comps.end(); ++it) {
            entities = entities.get() & it->get();
        }
    }

    EntitiesWrapper entities = EMPTY_ARRAY;
};

template<>
struct FilteredEntities<AND<Components<>>> {
    FilteredEntities(const World& /*unused*/) {}

    EntitiesWrapper entities = EMPTY_ARRAY;
};


template<typename... Component>
struct FilteredEntities<OR<Components<Component...>>> {
    FilteredEntities(const World& world) {
        static std::vector<EntitiesWrapper> ents_by_comps{world.entities<Component>()...};
        std::ranges::sort(ents_by_comps, std::less<>{}, [](const EntitiesWrapper& v) { return v.get().size(); });

        entities = ents_by_comps.begin()->get();
        for (auto it = std::next(ents_by_comps.begin()); it != ents_by_comps.end(); ++it) {
            entities = entities.get() | it->get();
        }
    }

    EntitiesWrapper entities = EMPTY_ARRAY;
};

template<>
struct FilteredEntities<OR<Components<>>> {
    FilteredEntities(const World& /*unused*/) {}

    EntitiesWrapper entities = EMPTY_ARRAY;
};

template<typename... T>
struct SortedEntities;

template<typename... Component>
struct SortedEntities<Components<Component...>> {
    SortedEntities(EntitiesWrapper entities, const World& world)
      : entities(((entities & world.entities<Component>()) + ...) + entities) {}

    EntitiesWrapper entities;
};

template<>
struct SortedEntities<Components<>> {
    SortedEntities(EntitiesWrapper entities, const World& /*world*/) : entities(entities) {}

    EntitiesWrapper entities;
};