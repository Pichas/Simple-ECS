#pragma once

#include "simple-ecs/entity_iterator.h"
#include "simple-ecs/filter.h"
#include "simple-ecs/world.h"


#define OBSERVER(Filter) const Observer<Filter>&
#define OBSERVER_EMPTY const Observer<>&

template<typename Filter>
struct Observer;

namespace detail::observer
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
        ArchetypeConstructor<typename std::remove_cvref_t<Archetype>::Components>::fill(
          observer, e, std::forward<Archetype>(obj));
    };
};

} // namespace detail::observer


template<typename Filter = RunEveryFrame>
struct Observer final : NoCopyNoMove {
    friend struct ObserverManager;

    using Require = typename Filter::Require::Type;
    using Exclude = typename Filter::Exclude::Type;

    Observer(World& world) : m_world(world) { refresh(); }
    ~Observer() noexcept = default;

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

    std::span<const Entity> entities() const noexcept { return *this; }
    operator std::span<const Entity>() const noexcept {
        std::lock_guard _(m_mutex);
        return {m_entities.cbegin(), m_entities.cend()};
    }

    ECS_FORCEINLINE decltype(auto) operator[](std::size_t index) const {
        std::lock_guard _(m_mutex);
        assert(index >= 0 && index < m_entities.size() && "Out of bound");
        return EntityWrapper(m_entities[index], *this);
    }

    void destroy() const { m_world.destroy(*this); }

    template<EcsTarget Target>
    bool isAlive(Target target) const noexcept {
        return m_world.isAlive(target);
    }

    template<EcsTarget Target>
    void destroy(Target target) const {
        m_world.destroy(target);
    }

    ECS_FORCEINLINE decltype(auto) create() const {
        ECS_PROFILER(ZoneScoped);
        return EntityWrapper(m_world.create(), *this);
    }

    template<typename Archetype>
    ECS_FORCEINLINE decltype(auto) create(Archetype&& obj = {}) const {
        ECS_PROFILER(ZoneScoped);

        Entity e = m_world.create();
        detail::observer::ArchetypeConstructor<Archetype>::fill(*this, e, std::forward<Archetype>(obj));
        return EntityWrapper(e, *this);
    }

    template<typename Component, EcsTarget Target>
    ECS_FORCEINLINE bool has(Target target) const noexcept {
        ECS_PROFILER(ZoneScoped);

        static_assert(ANY_OF<Components<Component>, Exclude>, "Component is in the Exclude list");
        return m_world.has<Component>(target);
    }

    template<typename Component, EcsTarget Target>
    requires(!std::is_empty_v<Component>)
    ECS_FORCEINLINE void emplace(Target target, Component&& c) const {
        ECS_PROFILER(ZoneScoped);

        m_world.emplace<Component>(target, std::forward<Component>(c));
    }

    template<typename... Component, typename... Args, EcsTarget Target>
    ECS_FORCEINLINE void emplace(Target targets, Args&&... args) const {
        ECS_PROFILER(ZoneScoped);

        (m_world.emplace<Component>(targets, std::forward<Args>(args)...), ...);
    }

    template<typename Component, EcsTarget Target>
    requires(!std::is_empty_v<Component>)
    ECS_FORCEINLINE void emplaceTagged(Target target, Component&& c) const {
        ECS_PROFILER(ZoneScoped);

        m_world.emplaceTagged<Component>(target, std::forward<Component>(c));
    }

    template<typename... Component, typename... Args, EcsTarget Target>
    ECS_FORCEINLINE void emplaceTagged(Target target, Args&&... args) const {
        ECS_PROFILER(ZoneScoped);

        (m_world.emplaceTagged<Component>(target, std::forward<Args>(args)...), ...);
    }

    template<typename... Component, EcsTarget Target>
    ECS_FORCEINLINE void markUpdated(Target target) const {
        ECS_PROFILER(ZoneScoped);

        static_assert(ALL_OF<Components<Component...>, Require>, "Component is not in the Require list");
        static_assert(ANY_OF<Components<Component...>, Exclude>, "Component is in the Exclude list");
        (m_world.markUpdated<Component>(target), ...);
    }

    template<typename... Component>
    ECS_FORCEINLINE void markUpdated() const {
        ECS_PROFILER(ZoneScoped);

        static_assert(ALL_OF<Components<Component...>, Require>, "Component is not in the Require list");
        static_assert(ANY_OF<Components<Component...>, Exclude>, "Component is in the Exclude list");
        (m_world.markUpdated<Component>(entities()), ...);
    }

    template<typename... Component, EcsTarget Target>
    ECS_FORCEINLINE void clearUpdateTag(Target target) const {
        ECS_PROFILER(ZoneScoped);

        static_assert(ANY_OF<Components<Component...>, Exclude>, "Component is in the Exclude list");
        (m_world.clearUpdateTag<Component>(target), ...);
    }

    template<typename... Component>
    ECS_FORCEINLINE void clearUpdateTag() const {
        ECS_PROFILER(ZoneScoped);

        static_assert(ANY_OF<Components<Component...>, Exclude>, "Component is in the Exclude list");
        (m_world.clearUpdateTag<Component>(entities()), ...);
    }

    template<typename... Component, EcsTarget Target>
    ECS_FORCEINLINE void erase(Target target) const {
        ECS_PROFILER(ZoneScoped);

        static_assert(ANY_OF<Components<Component...>, Exclude>, "Component is in the Exclude list");
        (m_world.erase<Component>(target), ...);
    }


    template<typename... Component>
    ECS_FORCEINLINE void erase() const {
        ECS_PROFILER(ZoneScoped);

        static_assert(ANY_OF<Components<Component...>, Exclude>, "Component is in the Exclude list");
        (m_world.erase<Component>(entities()), ...);
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
    void refresh() {
        ECS_PROFILER(ZoneScoped);

        m_world.notify(m_entities);

        auto filtered = FilteredEntities<AND<Require>>::ents(m_world);
        auto excluded = FilteredEntities<OR<Exclude>>::ents(m_world);
        auto result   = std::move(filtered) - std::move(excluded);

        std::lock_guard _(m_mutex);
        m_entities.swap(*result);
    }

private:
    World&              m_world;
    std::vector<Entity> m_entities;

    ECS_PROFILER(mutable TracyLockable(std::mutex, m_mutex));
    ECS_NO_PROFILER(mutable std::mutex m_mutex);
};