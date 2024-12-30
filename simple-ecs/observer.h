#pragma once

#include "simple-ecs/entity_iterator.h"
#include "simple-ecs/filter.h"
#include "simple-ecs/world.h"


#define OBSERVER(Filter) const Observer<Filter>&
#define OBSERVER_EMPTY const Observer<>&

template<typename Filter>
struct Observer;

namespace detail
{

template<typename...>
struct ArchetypeConstructor;

template<typename... Component>
struct ArchetypeConstructor<Components<Component...>> {
    template<typename Filter, typename Type>
    static void fill(const Observer<Filter>& observer, Entity e, Type&& obj) {
        static_assert(std::derived_from<std::remove_cvref_t<Type>, Archetype<Component...>>);

        auto emplace =
          []<typename OneComponent, typename ObjType>(const Observer<Filter>& observer, Entity e, ObjType&& obj) {
              if constexpr (!std::is_empty_v<OneComponent>) {
                  observer.emplace(e, OneComponent{static_cast<OneComponent>(std::forward<ObjType>(obj))});
              } else {
                  observer.template emplace<OneComponent>(e);
              }
          };
        (emplace.template operator()<Component>(observer, e, std::forward<Type>(obj)), ...);
    };
};

template<typename Archetype>
struct ArchetypeConstructor<Archetype> {
    template<typename Filter>
    static void fill(const Observer<Filter>& observer, Entity e, Archetype&& obj) {
        detail::ArchetypeConstructor<typename std::remove_cvref_t<Archetype>::Components>::fill(
          observer, e, std::forward<Archetype>(obj));
    };
};

} // namespace detail


template<typename Filter = RunEveryFrame>
struct Observer final : NoCopyNoMove {
    using Require = typename Filter::Require::Type;
    using Exclude = typename Filter::Exclude::Type;

    Observer(World& world) : m_world(world) {
        FilteredEntities<AND<Require>> filtered(m_world);
        FilteredEntities<OR<Exclude>>  excluded(m_world);
        m_entities = filtered.entities.get() - excluded.entities.get();
    }

    ~Observer() noexcept { m_world.notify(m_entities); }

    Registry* getRegistry() const noexcept { return m_world.getRegistry(); }

    decltype(auto) begin() const noexcept { return EntityIterator(m_entities.begin(), *this); }
    decltype(auto) end() const noexcept { return EntityIterator(m_entities.end(), *this); }
    decltype(auto) size() const noexcept { return m_entities.size(); }
    decltype(auto) empty() const noexcept { return m_entities.empty(); }

    operator std::span<const Entity>() const noexcept { return {m_entities.cbegin(), m_entities.cend()}; }

    ECS_FORCEINLINE decltype(auto) operator[](std::size_t index) const {
        assert(index >= 0 && index < m_entities.size() && "Out of bound");
        return EntityWrapper(m_entities[index], *this);
    }

    bool isAlive(Entity e) const noexcept { return m_world.isAlive(e); }
    void destroy(Entity e) const { m_world.destroy(e); }
    void destroy(std::span<const Entity> ents) const { m_world.destroy(ents); }

    ECS_FORCEINLINE decltype(auto) create() const { return EntityWrapper(m_world.create(), *this); }

    template<typename Archetype>
    ECS_FORCEINLINE decltype(auto) create(Archetype&& obj = {}) const {
        Entity e = m_world.create();
        detail::ArchetypeConstructor<Archetype>::fill(*this, e, std::forward<Archetype>(obj));
        return EntityWrapper(e, *this);
    }

    template<typename Component>
    ECS_FORCEINLINE bool has(Entity e) const noexcept {
        static_assert(ANY_OF<Components<Component>, Exclude>, "Component is in the Exclude list");
        return m_world.has<Component>(e);
    }

    template<typename Component>
    requires(!std::is_empty_v<Component>)
    ECS_FORCEINLINE void emplace(Entity e, Component&& c) const {
        m_world.emplace<Component>(e, std::forward<Component>(c));
    }

    template<typename Component, typename... Args>
    ECS_FORCEINLINE void emplace(Entity e, Args&&... args) const {
        m_world.emplace<Component>(e, std::forward<Args>(args)...);
    }

    template<typename Component>
    requires(!std::is_empty_v<Component>)
    ECS_FORCEINLINE void emplaceTagged(Entity e, Component&& c) const {
        m_world.emplaceTagged<Component>(e, std::forward<Component>(c));
    }

    template<typename Component, typename... Args>
    ECS_FORCEINLINE void emplaceTagged(Entity e, Args&&... args) const {
        m_world.emplaceTagged<Component>(e, std::forward<Args>(args)...);
    }

    template<typename... Component>
    ECS_FORCEINLINE void markUpdated(Entity e) const {
        static_assert(ALL_OF<Components<Component...>, Require>, "Component is not in the Require list");
        static_assert(ANY_OF<Components<Component...>, Exclude>, "Component is in the Exclude list");
        (m_world.markUpdated<Component>(e), ...);
    }

    template<typename... Component>
    ECS_FORCEINLINE void clearUpdateTag(Entity e) const {
        static_assert(ANY_OF<Components<Component...>, Exclude>, "Component is in the Exclude list");
        (m_world.clearUpdateTag<Component>(e), ...);
    }

    template<typename... Component>
    ECS_FORCEINLINE void clearUpdateTag(std::span<const Entity> ents) const {
        static_assert(ANY_OF<Components<Component...>, Exclude>, "Component is in the Exclude list");
        (m_world.clearUpdateTag<Component>(ents), ...);
    }

    template<typename... Component>
    ECS_FORCEINLINE void erase(Entity e) const {
        static_assert(ANY_OF<Components<Component...>, Exclude>, "Component is in the Exclude list");
        (m_world.erase<Component>(e), ...);
    }

    template<typename... Component>
    ECS_FORCEINLINE void erase(std::span<const Entity> ents) const {
        static_assert(ANY_OF<Components<Component...>, Exclude>, "Component is in the Exclude list");
        (m_world.erase<Component>(ents), ...);
    }

    template<typename... Component>
    ECS_FORCEINLINE void erase() const {
        static_assert(ANY_OF<Components<Component...>, Exclude>, "Component is in the Exclude list");
        (m_world.erase<Component>(*this), ...);
    }

    template<typename Component>
    requires(!std::is_empty_v<Component>)
    [[nodiscard]] ECS_FORCEINLINE decltype(auto) get(Entity e) const noexcept {
        static_assert(ALL_OF<Components<Component>, Require>, "Component is not in the Require list");
        static_assert(ANY_OF<Components<Component>, Exclude>, "Component is in the Exclude list");
        return m_world.get<Component>(e);
    }

    template<typename Component>
    requires(!std::is_empty_v<Component>)
    [[nodiscard]] ECS_FORCEINLINE decltype(auto) tryGet(Entity e) const noexcept {
        static_assert(ANY_OF<Components<Component>, Exclude>, "Component is in the Exclude list");
        return m_world.tryGet<Component>(e);
    }

private:
    World&                            m_world;
    static inline std::vector<Entity> m_entities;
};