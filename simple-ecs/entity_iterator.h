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

template<typename Observer, typename... Component>
struct ComponentsTuple<Observer, Components<Component...>> {
    using Type = std::tuple<RemoveTag_t<Component>&...>;
    static Type create(const Observer& observer, Entity e) {
        return {observer.template get<RemoveTag_t<Component>>(e)...};
    }

    using ConstType = std::tuple<const RemoveTag_t<Component>&...>;
    static ConstType createConst(const Observer& observer, Entity e) {
        return {observer.template get<RemoveTag_t<Component>>(e)...};
    }
};
} // namespace detail


template<typename Observer>
struct EntityWrapper final {
    using Require = typename Observer::Require;

    EntityWrapper(Entity e, const Observer& observer) : m_entity(e), m_observer(observer) {}
    EntityWrapper(const EntityWrapper&)                = delete;
    EntityWrapper(EntityWrapper&&) noexcept            = delete;
    EntityWrapper& operator=(const EntityWrapper&)     = delete;
    EntityWrapper& operator=(EntityWrapper&&) noexcept = delete;
    ~EntityWrapper() noexcept                          = default;

    operator Entity() const noexcept { return m_entity; }
    decltype(auto) asTuple() const { return detail::ComponentsTuple<Observer, Require>::create(m_observer, m_entity); }
    decltype(auto) asConstTuple() const {
        return detail::ComponentsTuple<Observer, Require>::createConst(m_observer, m_entity);
    }
    bool isAlive() const noexcept { return m_observer.isAlive(*this); }

    template<typename Component>
    requires(!std::is_empty_v<Component>)
    ECS_FORCEINLINE void emplace(Component&& c) const {
        m_observer.template emplace<Component>(*this, std::forward<Component>(c));
    }

    template<typename Component, typename... Args>
    ECS_FORCEINLINE void emplace(Args&&... args) const {
        m_observer.template emplace<Component>(*this, std::forward<Args>(args)...);
    }

    template<typename Component>
    ECS_FORCEINLINE bool has() const noexcept {
        return m_observer.template has<Component>(*this);
    }

    template<typename Component>
    requires(!std::is_empty_v<Component>)
    ECS_FORCEINLINE void emplaceTagged(Component&& c) const {
        m_observer.template emplaceTagged<Component>(*this, std::forward<Component>(c));
    }

    template<typename Component, typename... Args>
    ECS_FORCEINLINE void emplaceTagged(Args&&... args) const {
        m_observer.template emplaceTagged<Component>(*this, std::forward<Args>(args)...);
    }

    template<typename Component>
    ECS_FORCEINLINE void markUpdated() const {
        m_observer.template markUpdated<Component>(*this);
    }

    template<typename Component>
    ECS_FORCEINLINE void clearUpdateTag() const {
        m_observer.template clearUpdateTag<Component>(*this);
    }

    template<typename Component>
    ECS_FORCEINLINE void erase() const {
        return m_observer.template erase<Component>(*this);
    }

    template<typename Component>
    requires(!std::is_empty_v<Component>)
    [[nodiscard]] ECS_FORCEINLINE decltype(auto) get() const noexcept {
        return m_observer.template get<Component>(*this);
    }

    template<typename Component>
    requires(!std::is_empty_v<Component>)
    [[nodiscard]] ECS_FORCEINLINE decltype(auto) tryGet() const noexcept {
        return m_observer.template tryGet<Component>(*this);
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
    using pointer           = value_type;
    using reference         = value_type;

    EntityIterator(EntityContainerIt container_iterator, const Observer& observer)
      : m_it(std::move(container_iterator)), m_observer(observer) {}

    EntityIterator(const EntityIterator& other)                = default;
    EntityIterator(EntityIterator&& other) noexcept            = default;
    EntityIterator& operator=(const EntityIterator& other)     = default;
    EntityIterator& operator=(EntityIterator&& other) noexcept = default;
    ~EntityIterator() noexcept                                 = default;

    bool operator==(const EntityIterator& other) const noexcept { return m_it == other.m_it; };
    bool operator<(const EntityIterator& other) const noexcept { return m_it < other.m_it; };

    reference operator*() const { return value_type(*m_it, m_observer); }

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