#pragma once

#include "simple-ecs/components.h"
#include "simple-ecs/entity.h"
#include "simple-ecs/storage.h"
#include "simple-ecs/utils.h"

#include <algorithm>
#include <cassert>
#include <list>
#include <map>
#include <memory>
#include <ranges>
#include <span>
#include <vector>


namespace detail
{
inline IDType nextID() {
    static IDType id = 0;
    return id++;
}

template<typename T>
inline IDType sequenceID() {
    static IDType id = nextID();
    return id;
};
} // namespace detail

struct Registry;

struct World final : NoCopyNoMove {
    explicit World();

    Registry* getRegistry() const noexcept { return m_reg.get(); }

    decltype(auto) begin() const noexcept { return m_entities.cbegin(); }
    decltype(auto) end() const noexcept { return m_entities.cend(); }
    decltype(auto) size() const noexcept { return m_entities.size(); }

    const std::vector<Entity>&              entities() const noexcept { return m_entities; }
    const std::map<std::string, Component>& registeredComponentNames() const noexcept { return m_component_name; }
    bool isAlive(Entity e) const noexcept { return std::ranges::binary_search(m_entities, e); }

    template<typename Component, typename Callback>
    void addDestroyCallback(Callback&& f) {
        ECS_ASSERT(m_storages.size() > detail::sequenceID<Component>(), "Storage doesn't exist");
        auto& storage = m_storages.at(detail::sequenceID<Component>());

        if constexpr (std::is_empty_v<Component>) {
            static_assert(std::is_invocable_r_v<void, Callback, Entity>);
            storage.template addDestroyCallback<Component, Callback, Entity>(std::forward<Callback>(f));
        } else {
            static_assert(std::is_invocable_r_v<void, Callback, Entity, Component&>);
            storage.template addDestroyCallback<Component, Callback, Entity, Component&>(std::forward<Callback>(f));
        }
    }

    template<typename Component, typename Callback>
    void addConstructCallback(Callback&& f) {
        ECS_ASSERT(m_storages.size() > detail::sequenceID<Component>(), "Storage doesn't exist");
        auto& storage = m_storages.at(detail::sequenceID<Component>());

        if constexpr (std::is_empty_v<Component>) {
            static_assert(std::is_invocable_r_v<void, Callback, Entity>);
            storage.template addConstructCallback<Component, Callback, Entity>(std::forward<Callback>(f));
        } else {
            static_assert(std::is_invocable_r_v<void, Callback, Entity, Component&>);
            storage.template addConstructCallback<Component, Callback, Entity, Component&>(std::forward<Callback>(f));
        }
    }

    template<typename Component>
    decltype(auto) size() const noexcept {
        ECS_ASSERT(m_storages.size() > detail::sequenceID<Component>(), "Storage doesn't exist");
        return m_storages.at(detail::sequenceID<Component>())->size();
    }

    template<typename Component>
    void createStorage() {
        using Tag = Updated<Component>;

        auto add_storage = [this]<typename T>() {
            // generate runtime ID for components.
            std::ignore = detail::sequenceID<T>();
            ECS_ASSERT(m_storages.size() == detail::sequenceID<T>(), "Storage already exists");
            m_storages.emplace_back().setType<T>();
        };

        add_storage.template operator()<Component>();
        add_storage.template operator()<Tag>();

        m_component_name.emplace(ct::name<Component>, ct::ID<Component>);
    }

    template<typename Component>
    ECS_FORCEINLINE bool has(Entity e) noexcept {
        ECS_ASSERT(m_storages.size() > detail::sequenceID<Component>(), "Storage doesn't exist");
        ECS_ASSERT(isAlive(e), "Entity doesn't exist");
        return m_storages.at(detail::sequenceID<Component>()).has(e);
    }

    template<typename Component, typename Type = std::remove_cvref_t<Component>>
    requires(!std::is_empty_v<Type>)
    ECS_FORCEINLINE void emplace(Entity e, Component&& c) {
        ECS_ASSERT(m_storages.size() > detail::sequenceID<Type>(), "Storage doesn't exist");
        ECS_ASSERT(isAlive(e), "Entity doesn't exist");
        auto& storage = m_storages.at(detail::sequenceID<Type>());
        storage.template emplace<Type>(e, std::forward<Component>(c));
        notify(e);
    }

    template<typename Component, typename... Args>
    ECS_FORCEINLINE void emplace(Entity e, Args&&... args) {
        ECS_ASSERT(m_storages.size() > detail::sequenceID<Component>(), "Storage doesn't exist");
        ECS_ASSERT(isAlive(e), "Entity doesn't exist");
        auto& storage = m_storages.at(detail::sequenceID<Component>());
        storage.template emplace<Component>(e, std::forward<Args>(args)...);
        notify(e);
    }

    template<typename Component, typename Type = std::remove_cvref_t<Component>>
    requires(!std::is_empty_v<Type>)
    ECS_FORCEINLINE void forceEmplace(Entity e, Component&& c) {
        ECS_ASSERT(m_storages.size() > detail::sequenceID<Type>(), "Storage doesn't exist");
        ECS_ASSERT(isAlive(e), "Entity doesn't exist");
        auto& storage = m_storages.at(detail::sequenceID<Type>());
        storage.template erase<Type>(e);
        storage.template emplace<Type>(e, std::forward<Component>(c));
        notify(e);
    }

    template<typename Component, typename... Args>
    ECS_FORCEINLINE void forceEmplace(Entity e, Args&&... args) {
        ECS_ASSERT(m_storages.size() > detail::sequenceID<Component>(), "Storage doesn't exist");
        ECS_ASSERT(isAlive(e), "Entity doesn't exist");
        auto& storage = m_storages.at(detail::sequenceID<Component>());
        storage.template erase<Component>(e);
        storage.template emplace<Component>(e, std::forward<Args>(args)...);
        notify(e);
    }

    template<typename Component, typename Type = std::remove_cvref_t<Component>>
    requires(!std::is_empty_v<Type>)
    ECS_FORCEINLINE void emplaceTagged(Entity e, Component&& c) {
        emplace<Type>(e, std::forward<Component>(c));
        markUpdated<Type>(e);
    }

    template<typename Component, typename... Args>
    ECS_FORCEINLINE void emplaceTagged(Entity e, Args&&... args) {
        emplace<Component>(e, std::forward<Args>(args)...);
        markUpdated<Component>(e);
    }

    template<typename Component>
    ECS_FORCEINLINE void markUpdated(Entity e) {
        using Tag = Updated<Component>;
        ECS_ASSERT(has<Component>(e), "Entity should have Component before you can marked it as Updated");
        emplace<Tag>(e);
    }

    template<typename Component>
    ECS_FORCEINLINE void clearUpdateTag(Entity e) {
        using Tag = Updated<Component>;
        erase<Tag>(e);
    }

    template<typename Component>
    ECS_FORCEINLINE void clearUpdateTag(std::span<const Entity> ents) {
        using Tag = Updated<Component>;
        erase<Tag>(ents);
    }

    template<typename Component>
    ECS_FORCEINLINE void erase(Entity e) {
        ECS_ASSERT(m_storages.size() > detail::sequenceID<Component>(), "Storage doesn't exist");
        ECS_ASSERT(isAlive(e), "Entity doesn't exist");
        auto& storage = m_storages.at(detail::sequenceID<Component>());
        storage.template erase<Component>(e);
        notify(e);
    }

    template<typename Component>
    ECS_FORCEINLINE void erase(std::span<const Entity> ents) {
        ECS_ASSERT(m_storages.size() > detail::sequenceID<Component>(), "Storage doesn't exist");
        ECS_ASSERT(std::ranges::all_of(ents, [this](const Entity& e) { return isAlive(e); }), "Entity doesn't exist");
        auto& storage = m_storages.at(detail::sequenceID<Component>());
        storage.template erase<Component>(ents);
        notify(ents);
    }

    template<typename Component>
    requires(!std::is_empty_v<Component>)
    [[nodiscard]] ECS_FORCEINLINE decltype(auto) get(Entity e) noexcept {
        ECS_ASSERT(m_storages.size() > detail::sequenceID<Component>(), "Storage doesn't exist");
        ECS_ASSERT(isAlive(e), "Entity doesn't exist");
        auto& storage = m_storages.at(detail::sequenceID<Component>());
        return storage.template get<Component>(e);
    }

    template<typename Component>
    requires(!std::is_empty_v<Component>)
    [[nodiscard]] ECS_FORCEINLINE decltype(auto) tryGet(Entity e) noexcept {
        ECS_ASSERT(m_storages.size() > detail::sequenceID<Component>(), "Storage doesn't exist");
        ECS_ASSERT(isAlive(e), "Entity doesn't exist");
        auto& storage = m_storages.at(detail::sequenceID<Component>());
        return storage.template tryGet<Component>(e);
    }

    template<typename Component>
    [[nodiscard]] const std::vector<Entity>& entities() const noexcept {
        ECS_ASSERT(m_storages.size() > detail::sequenceID<Component>(), "Storage doesn't exist");
        return m_storages.at(detail::sequenceID<Component>()).entities();
    }

    [[nodiscard]] Entity create() {
        if (m_free_entities.empty()) {
            m_free_entities.emplace_back(m_entities.size());
        }

        Entity entity = 0;
        if (m_free_entities.front() > m_free_entities.back()) {
            entity = m_free_entities.front();
            m_free_entities.pop_front();
        } else {
            entity = m_free_entities.back();
            m_free_entities.pop_back();
        }

        auto lower = std::ranges::lower_bound(m_entities, entity);
        m_entities.insert(lower, entity);

        notify(entity);
        return entity;
    }

    void destroy(Entity e) { m_entities_to_destroy.emplace_back(e); }

    void destroy(std::span<const Entity> entities) {
        for (auto entity : entities) {
            m_entities_to_destroy.emplace_back(entity);
        }
    }

    void flush() {
        for (auto entity : std::views::reverse(m_entities_to_destroy)) {
            ECS_ASSERT(isAlive(entity), "Entity doesn't exist");
            for (auto& storage : m_storages) {
                storage.remove(entity);
            }

            std::erase(m_entities, entity);
            if (!m_free_entities.empty() && m_free_entities.front() > entity) {
                m_free_entities.emplace_back(entity);
            } else {
                m_free_entities.emplace_front(entity);
            }

            notify(entity);
        }
        m_entities_to_destroy.clear();
    }

    std::vector<std::string> componentsNames(Entity e) {
        ECS_RELEASE_ONLY(
          spdlog::warn("Don't use this method in release build. We don't have storage names in release build"));
        ECS_ASSERT(isAlive(e), "Entity doesn't exist");
        std::vector<std::string> names;
        for (const auto& storage : m_storages) {
            if (storage.has(e)) {
                ECS_DEBUG_ONLY(names.emplace_back(storage.name()));
            }
        }

        return names;
    }

    void subscribe(std::function<void(Entity)> func) { m_notify_callback.emplace_back(std::move(func)); }

    void notify(Entity entity) {
        for (const auto& func : m_notify_callback) {
            func(entity);
        }
    }

    void notify(std::span<const Entity> entities) {
        for (const auto& func : m_notify_callback) {
            for (auto entity : entities) {
                func(entity);
            }
        }
    }

private:
    std::unique_ptr<Registry>                m_reg;
    std::vector<Entity>                      m_entities;
    std::vector<Entity>                      m_entities_to_destroy;
    std::vector<Storage>                     m_storages;
    std::vector<std::function<void(Entity)>> m_notify_callback;
    std::list<Entity>                        m_free_entities;
    std::map<std::string, Component>         m_component_name;
};