#pragma once

#include "simple-ecs/components.h"
#include "simple-ecs/entity.h"

#include "utils.h"
#include <iterator>
#include <tuple>

namespace detail
{

template<typename... Component>
struct ComponentsTuple;

template<typename... Component>
struct ComponentsTuple<Components<Component...>> {
    template<typename EntityWrapper>
    static auto create(EntityWrapper&& e) {
        return std::make_tuple<Component&...>(std::forward<EntityWrapper>(e).template get<Component>()...);
    }
};

} // namespace detail


template<typename Observer>
struct EntityWrapper final {
    using Require = typename Observer::Require;

    EntityWrapper(Entity e, const Observer& observer) : m_entity(e), m_observer(observer) {}
    EntityWrapper(const EntityWrapper&)                = default;
    EntityWrapper(EntityWrapper&&) noexcept            = default;
    EntityWrapper& operator=(const EntityWrapper&)     = default;
    EntityWrapper& operator=(EntityWrapper&&) noexcept = default;
    ~EntityWrapper() noexcept                          = default;

    operator Entity() const noexcept { return m_entity; }
    Entity entity() const noexcept { return m_entity; }

    decltype(auto) get() const { return detail::ComponentsTuple<RemoveEmpty_t<RemoveTags_t<Require>>>::create(*this); }

    ECS_FORCEINLINE bool isAlive() const noexcept { return m_observer.isAlive(m_entity); }
    ECS_FORCEINLINE void destroy() const { m_observer.destroy(m_entity); }

    template<typename Component>
    ECS_FORCEINLINE bool has() const noexcept {
        return m_observer.template has<Component>(m_entity);
    }

    template<typename Component>
    requires(!std::is_empty_v<Component>)
    ECS_FORCEINLINE void emplace(Component&& c) const {
        m_observer.template emplace<Component>(m_entity, std::forward<Component>(c));
    }

    template<typename... Component, typename... Args>
    ECS_FORCEINLINE void emplace(Args&&... args) const {
        m_observer.template emplace<Component...>(m_entity, std::forward<Args>(args)...);
    }

    template<typename Component>
    requires(!std::is_empty_v<Component>)
    ECS_FORCEINLINE void emplaceTagged(Component&& c) const {
        m_observer.template emplaceTagged<Component>(m_entity, std::forward<Component>(c));
    }

    template<typename... Component, typename... Args>
    ECS_FORCEINLINE void emplaceTagged(Args&&... args) const {
        m_observer.template emplaceTagged<Component...>(m_entity, std::forward<Args>(args)...);
    }

    template<typename... Component>
    ECS_FORCEINLINE void markUpdated() const {
        m_observer.template markUpdated<Component...>(m_entity);
    }

    template<typename... Component>
    ECS_FORCEINLINE void clearUpdateTag() const {
        m_observer.template clearUpdateTag<Component...>(m_entity);
    }

    template<typename... Component>
    ECS_FORCEINLINE void erase() const {
        m_observer.template erase<Component...>(m_entity);
    }

    template<typename Component>
    requires(!std::is_empty_v<Component>)
    [[nodiscard]] ECS_FORCEINLINE decltype(auto) get() const noexcept {
        return m_observer.template get<Component>(m_entity);
    }

    template<typename Component>
    requires(!std::is_empty_v<Component>)
    [[nodiscard]] ECS_FORCEINLINE decltype(auto) tryGet() const noexcept {
        return m_observer.template tryGet<Component>(m_entity);
    }

private:
    const Entity    m_entity;
    const Observer& m_observer;
};


template<typename EntityContainerIt, typename Observer>
struct EntityIterator final {
    using iterator_concept  = std::contiguous_iterator_tag;
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type        = EntityWrapper<Observer>;
    using difference_type   = std::size_t;
    using pointer           = value_type*;
    using reference         = value_type&;

    EntityIterator(EntityContainerIt container_iterator, const Observer& observer)
      : m_it(std::move(container_iterator)), m_observer(observer) {}

    EntityIterator(const EntityIterator& other)                = default;
    EntityIterator(EntityIterator&& other) noexcept            = default;
    EntityIterator& operator=(const EntityIterator& other)     = default;
    EntityIterator& operator=(EntityIterator&& other) noexcept = default;
    ~EntityIterator() noexcept                                 = default;

    bool operator==(const EntityIterator& other) const noexcept { return m_it == other.m_it; };
    bool operator<(const EntityIterator& other) const noexcept { return m_it < other.m_it; };

    value_type operator*() const { return {*m_it, m_observer}; }

    EntityIterator& operator++() {
        ++m_it;
        return *this;
    }
    EntityIterator operator++(int) { // NOLINT
        EntityIterator temp = *this;
        ++m_it;
        return temp;
    }
    EntityIterator& operator--() {
        --m_it;
        return *this;
    }
    EntityIterator operator--(int) { // NOLINT
        EntityIterator temp = *this;
        --m_it;
        return temp;
    }

private:
    EntityContainerIt m_it;
    const Observer&   m_observer;
};