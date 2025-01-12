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
    static void fill(OBSERVER(Filter) observer, Entity e, Type&& obj) {
        static_assert(std::derived_from<std::remove_cvref_t<Type>, Archetype<Component...>>);

        auto emplace = []<typename OneComponent, typename ObjType>(OBSERVER(Filter) observer, Entity e, ObjType&& obj) {
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
    static void fill(OBSERVER(Filter) observer, Entity e, Archetype&& obj) {
        detail::ArchetypeConstructor<typename std::remove_cvref_t<Archetype>::Components>::fill(
          observer, e, std::forward<Archetype>(obj));
    };
};

} // namespace detail


template<typename Filter = RunEveryFrame>
struct Observer final : NoCopyNoMove {
    using Require = typename Filter::Require::Type;
    using Exclude = typename Filter::Exclude::Type;

    Observer(World& world) : m_world(world) { refresh(); }

    ~Observer() noexcept = default;

    void refresh() {
        ECS_PROFILER(ZoneScoped);

        m_world.notify(m_entities);

        auto filtered = FilteredEntities<AND<Require>>::ents(m_world);
        auto excluded = FilteredEntities<OR<Exclude>>::ents(m_world);
        auto result   = std::move(filtered) - std::move(excluded);

        std::lock_guard _(m_mutex);
        m_entities.swap(*result);
    }

    Registry* getRegistry() const noexcept { return m_world.getRegistry(); }

    decltype(auto) begin() const noexcept {
        std::lock_guard _(m_mutex);
        return EntityIterator(m_entities.cbegin(), *this);
    }
    decltype(auto) end() const noexcept {
        std::lock_guard _(m_mutex);
        return EntityIterator(m_entities.cend(), *this);
    }
    size_t size() const noexcept {
        std::lock_guard _(m_mutex);
        return m_entities.size();
    }
    bool empty() const noexcept {
        std::lock_guard _(m_mutex);
        return m_entities.empty();
    }

    operator std::span<const Entity>() const noexcept {
        std::lock_guard _(m_mutex);
        return {m_entities.cbegin(), m_entities.cend()};
    }

    ECS_FORCEINLINE decltype(auto) operator[](std::size_t index) const {
        std::lock_guard _(m_mutex);
        assert(index >= 0 && index < m_entities.size() && "Out of bound");
        return EntityWrapper(m_entities[index], *this);
    }

    bool isAlive(Entity e) const noexcept { return m_world.isAlive(e); }
    void destroy(Entity e) const { m_world.destroy(e); }
    void destroy(std::span<const Entity> ents) const { m_world.destroy(ents); }
    void destroy() const { m_world.destroy(*this); }

    ECS_FORCEINLINE decltype(auto) create() const {
        ECS_PROFILER(ZoneScoped);
        return EntityWrapper(m_world.create(), *this);
    }

    template<typename Archetype>
    ECS_FORCEINLINE decltype(auto) create(Archetype&& obj = {}) const {
        ECS_PROFILER(ZoneScoped);

        Entity e = m_world.create();
        detail::ArchetypeConstructor<Archetype>::fill(*this, e, std::forward<Archetype>(obj));
        return EntityWrapper(e, *this);
    }

    template<typename Component>
    ECS_FORCEINLINE bool has(Entity e) const noexcept {
        ECS_PROFILER(ZoneScoped);

        static_assert(ANY_OF<Components<Component>, Exclude>, "Component is in the Exclude list");
        return m_world.has<Component>(e);
    }

    template<typename Component>
    requires(!std::is_empty_v<Component>)
    ECS_FORCEINLINE void emplace(Entity e, Component&& c) const {
        ECS_PROFILER(ZoneScoped);

        m_world.emplace<Component>(e, std::forward<Component>(c));
    }

    template<typename Component, typename... Args>
    ECS_FORCEINLINE void emplace(Entity e, Args&&... args) const {
        ECS_PROFILER(ZoneScoped);

        m_world.emplace<Component>(e, std::forward<Args>(args)...);
    }

    template<typename Component>
    requires(!std::is_empty_v<Component>)
    ECS_FORCEINLINE void emplaceTagged(Entity e, Component&& c) const {
        ECS_PROFILER(ZoneScoped);

        m_world.emplaceTagged<Component>(e, std::forward<Component>(c));
    }

    template<typename Component, typename... Args>
    ECS_FORCEINLINE void emplaceTagged(Entity e, Args&&... args) const {
        ECS_PROFILER(ZoneScoped);

        m_world.emplaceTagged<Component>(e, std::forward<Args>(args)...);
    }

    template<typename... Component>
    ECS_FORCEINLINE void markUpdated(Entity e) const {
        ECS_PROFILER(ZoneScoped);

        static_assert(ALL_OF<Components<Component...>, Require>, "Component is not in the Require list");
        static_assert(ANY_OF<Components<Component...>, Exclude>, "Component is in the Exclude list");
        (m_world.markUpdated<Component>(e), ...);
    }

    template<typename... Component>
    ECS_FORCEINLINE void clearUpdateTag(Entity e) const {
        ECS_PROFILER(ZoneScoped);

        static_assert(ANY_OF<Components<Component...>, Exclude>, "Component is in the Exclude list");
        (m_world.clearUpdateTag<Component>(e), ...);
    }

    template<typename... Component>
    ECS_FORCEINLINE void clearUpdateTag(std::span<const Entity> ents) const {
        ECS_PROFILER(ZoneScoped);

        static_assert(ANY_OF<Components<Component...>, Exclude>, "Component is in the Exclude list");
        (m_world.clearUpdateTag<Component>(ents), ...);
    }

    template<typename... Component>
    ECS_FORCEINLINE void clearUpdateTag() const {
        ECS_PROFILER(ZoneScoped);

        static_assert(ANY_OF<Components<Component...>, Exclude>, "Component is in the Exclude list");
        (m_world.clearUpdateTag<Component>(*this), ...);
    }

    template<typename... Component>
    ECS_FORCEINLINE void erase(Entity e) const {
        ECS_PROFILER(ZoneScoped);

        static_assert(ANY_OF<Components<Component...>, Exclude>, "Component is in the Exclude list");
        (m_world.erase<Component>(e), ...);
    }

    template<typename... Component>
    ECS_FORCEINLINE void erase(std::span<const Entity> ents) const {
        ECS_PROFILER(ZoneScoped);

        static_assert(ANY_OF<Components<Component...>, Exclude>, "Component is in the Exclude list");
        (m_world.erase<Component>(ents), ...);
    }

    template<typename... Component>
    ECS_FORCEINLINE void erase() const {
        ECS_PROFILER(ZoneScoped);

        static_assert(ANY_OF<Components<Component...>, Exclude>, "Component is in the Exclude list");
        (m_world.erase<Component>(*this), ...);
    }

    template<typename Component>
    requires(!std::is_empty_v<Component>)
    [[nodiscard]] ECS_FORCEINLINE decltype(auto) get(Entity e) const noexcept {
        ECS_PROFILER(ZoneScoped);

        static_assert(ALL_OF<Components<Component>, Require>, "Component is not in the Require list");
        static_assert(ANY_OF<Components<Component>, Exclude>, "Component is in the Exclude list");
        return m_world.get<Component>(e);
    }

    template<typename Component>
    requires(!std::is_empty_v<Component>)
    [[nodiscard]] ECS_FORCEINLINE decltype(auto) tryGet(Entity e) const noexcept {
        ECS_PROFILER(ZoneScoped);

        static_assert(ANY_OF<Components<Component>, Exclude>, "Component is in the Exclude list");
        return m_world.tryGet<Component>(e);
    }

private:
    World&              m_world;
    std::vector<Entity> m_entities;

    ECS_PROFILER(mutable TracyLockable(std::mutex, m_mutex));
    ECS_NO_PROFILER(mutable std::mutex m_mutex);
};