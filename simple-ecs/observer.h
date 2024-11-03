#pragma once

#include "simple-ecs/entity_iterator.h"
#include "simple-ecs/filter.h"
#include "simple-ecs/world.h"


template<typename Filter>
struct Observer final {
    using Require = typename Filter::Require::Type;
    using Exclude = typename Filter::Exclude::Type;

    Observer(World& world) : m_world(world) {
        FilteredEntities<AND<Require>> filtered(m_world);
        FilteredEntities<OR<Exclude>>  excluded(m_world);
        m_entities = filtered.entities.get() - excluded.entities.get();
    }

    ~Observer() { ECS_DEBUG_ONLY(m_world.notify(m_entities)); }

    Registry* getRegistry() const noexcept { return m_world.getRegistry(); }

    ECS_FORCEINLINE decltype(auto) create() const { return EntityWrapper(m_world.create(), *this); }
    bool                           isAlive(Entity e) const noexcept { return m_world.isAlive(e); }
    void                           destroy(Entity e) const { m_world.destroy(e); }
    void                           destroy(std::span<const Entity> ents) const { m_world.destroy(ents); }

    operator std::span<const Entity>() const noexcept { return {m_entities.cbegin(), m_entities.cend()}; }

    decltype(auto) begin() const noexcept { return EntityIterator(m_entities.begin(), *this); }
    decltype(auto) end() const noexcept { return EntityIterator(m_entities.end(), *this); }
    decltype(auto) size() const noexcept { return m_entities.size(); }
    decltype(auto) empty() const noexcept { return m_entities.empty(); }


    template<typename Component>
    bool has(Entity e) const noexcept {
        static_assert(TYPE_EXCLUDED<Component, Exclude>, "Component is in the Exclude list");
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

    template<typename Component>
    ECS_FORCEINLINE void markUpdated(Entity e) const {
        static_assert(TYPE_REQUESTED<Component, Require>, "Component is not in the Require list");
        static_assert(TYPE_EXCLUDED<Component, Exclude>, "Component is in the Exclude list");
        m_world.markUpdated<Component>(e);
    }

    template<typename Component>
    ECS_FORCEINLINE void clearUpdateTag(Entity e) const {
        static_assert(TYPE_EXCLUDED<Component, Exclude>, "Component is in the Exclude list");
        m_world.clearUpdateTag<Component>(e);
    }

    template<typename Component>
    ECS_FORCEINLINE void clearUpdateTag(std::span<const Entity> ents) const {
        static_assert(TYPE_EXCLUDED<Component, Exclude>, "Component is in the Exclude list");
        m_world.clearUpdateTag<Component>(ents);
    }

    template<typename Component>
    ECS_FORCEINLINE void erase(Entity e) const {
        static_assert(TYPE_EXCLUDED<Component, Exclude>, "Component is in the Exclude list");
        return m_world.erase<Component>(e);
    }

    template<typename Component>
    ECS_FORCEINLINE void erase(std::span<const Entity> ents) const {
        static_assert(TYPE_EXCLUDED<Component, Exclude>, "Component is in the Exclude list");
        return m_world.erase<Component>(ents);
    }

    template<typename Component>
    requires(!std::is_empty_v<Component>)
    [[nodiscard]] ECS_FORCEINLINE decltype(auto) get(Entity e) const noexcept {
        static_assert(TYPE_REQUESTED<Component, Require>, "Component is not in the Require list");
        static_assert(TYPE_EXCLUDED<Component, Exclude>, "Component is in the Exclude list");
        ECS_ASSERT(m_world.isAlive(e), "Entity doesn't exist");
        return m_world.get<Component>(e);
    }

    template<typename Component>
    requires(!std::is_empty_v<Component>)
    [[nodiscard]] ECS_FORCEINLINE decltype(auto) tryGet(Entity e) const noexcept {
        static_assert(TYPE_EXCLUDED<Component, Exclude>, "Component is in the Exclude list");
        ECS_ASSERT(m_world.isAlive(e), "Entity doesn't exist");
        return m_world.tryGet<Component>(e);
    }

private:
    World&                            m_world;
    static inline std::vector<Entity> m_entities;
};