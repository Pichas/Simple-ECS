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
#include <span>
#include <vector>


namespace detail::world
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

} // namespace detail::world


struct Registry;

struct World final : NoCopyNoMove {
    explicit World();

    Registry* getRegistry() const noexcept { return m_reg.get(); }

    decltype(auto) begin() const noexcept { return m_entities.cbegin(); }
    decltype(auto) end() const noexcept { return m_entities.cend(); }
    decltype(auto) size() const noexcept { return m_entities.size(); }
    decltype(auto) empty() const noexcept { return m_entities.empty(); }

    std::size_t                             totalComponents() const noexcept { return m_storages.size(); }
    const std::vector<Entity>&              entities() const noexcept { return m_entities; }
    const std::map<std::string, Component>& registeredComponentNames() const noexcept { return m_component_name; }
    bool isAlive(Entity e) const noexcept { return std::ranges::binary_search(m_entities, e); }
    bool isAlive(std::span<const Entity> ents) const noexcept {
        bool result = true;
        for (auto e : ents) {
            result &= isAlive(e);
        }
        return result;
    }

    template<typename Component>
    void addEmplaceCallback(Storage<Component>::Callback&& f) {
        ECS_PROFILER(ZoneScoped);

        ECS_ASSERT(m_storages.size() > detail::world::sequenceID<Component>(), "Storage doesn't exist");
        auto* storage = static_cast<Storage<Component>*>(m_storages.at(detail::world::sequenceID<Component>()).get());
        storage->addEmplaceCallback(std::move(f));
    }

    template<typename Component>
    void addDestroyCallback(Storage<Component>::Callback&& f) {
        ECS_PROFILER(ZoneScoped);

        ECS_ASSERT(m_storages.size() > detail::world::sequenceID<Component>(), "Storage doesn't exist");
        auto* storage = static_cast<Storage<Component>*>(m_storages.at(detail::world::sequenceID<Component>()).get());
        storage->addDestroyCallback(std::move(f));
    }

    template<typename Component>
    decltype(auto) size() const noexcept {
        ECS_PROFILER(ZoneScoped);

        ECS_ASSERT(m_storages.size() > detail::world::sequenceID<Component>(), "Storage doesn't exist");
        auto* storage = static_cast<Storage<Component>*>(m_storages.at(detail::world::sequenceID<Component>()).get());
        return storage->size();
    }

    template<typename Component>
    decltype(auto) empty() const noexcept {
        ECS_PROFILER(ZoneScoped);

        ECS_ASSERT(m_storages.size() > detail::world::sequenceID<Component>(), "Storage doesn't exist");
        auto* storage = static_cast<Storage<Component>*>(m_storages.at(detail::world::sequenceID<Component>()).get());
        return storage->empty();
    }


    template<typename Component>
    void createStorage() {
        ECS_PROFILER(ZoneScoped);

        auto add_storage = [this]<typename T>() {
            // generate runtime ID for components.
            std::ignore = detail::world::sequenceID<T>();
            ECS_ASSERT(m_storages.size() == detail::world::sequenceID<T>(), "Storage already exists");
            m_storages.emplace_back(std::make_unique<Storage<T>>());
        };

        add_storage.template operator()<Component>();
        add_storage.template operator()<Updated<Component>>();

        auto [_, was_added] = m_component_name.try_emplace(std::string(ct::NAME<Component>), ct::ID<Component>);
        assert(was_added);
    }

    template<typename Component, EcsTarget Target>
    ECS_FORCEINLINE bool has(Target target) noexcept {
        ECS_PROFILER(ZoneScoped);

        ECS_ASSERT(m_storages.size() > detail::world::sequenceID<Component>(), "Storage doesn't exist");
        ECS_ASSERT(isAlive(target), "Entity doesn't exist");
        auto* storage = static_cast<Storage<Component>*>(m_storages.at(detail::world::sequenceID<Component>()).get());
        return storage->has(target);
    }


    template<typename Component, EcsTarget Target, typename Type = std::remove_cvref_t<Component>>
    requires(!std::is_empty_v<Type>)
    ECS_FORCEINLINE void emplace(Target target, Component&& c) {
        ECS_PROFILER(ZoneScoped);

        ECS_ASSERT(m_storages.size() > detail::world::sequenceID<Type>(), "Storage doesn't exist");
        ECS_ASSERT(isAlive(target), "Entity doesn't exist");
        auto* storage = static_cast<Storage<Type>*>(m_storages.at(detail::world::sequenceID<Type>()).get());
        storage->emplace(target, std::forward<Component>(c));
        notify(target);
    }

    template<typename Component, typename... Args, EcsTarget Target>
    ECS_FORCEINLINE void emplace(Target target, Args&&... args) {
        ECS_PROFILER(ZoneScoped);

        ECS_ASSERT(m_storages.size() > detail::world::sequenceID<Component>(), "Storage doesn't exist");
        ECS_ASSERT(isAlive(target), "Entity doesn't exist");
        auto* storage = static_cast<Storage<Component>*>(m_storages.at(detail::world::sequenceID<Component>()).get());
        storage->emplace(target, std::forward<Args>(args)...);
        notify(target);
    }

    template<typename Component, EcsTarget Target, typename Type = std::remove_cvref_t<Component>>
    requires(!std::is_empty_v<Type>)
    ECS_FORCEINLINE void emplaceTagged(Target target, Component&& c) {
        ECS_PROFILER(ZoneScoped);

        emplace<Type>(target, std::forward<Component>(c));
        markUpdated<Type>(target);
    }

    template<typename Component, typename... Args, EcsTarget Target>
    ECS_FORCEINLINE void emplaceTagged(Target target, Args&&... args) {
        ECS_PROFILER(ZoneScoped);

        emplace<Component>(target, std::forward<Args>(args)...);
        markUpdated<Component>(target);
    }

    template<typename Component, EcsTarget Target>
    ECS_FORCEINLINE void markUpdated(Target target) {
        ECS_PROFILER(ZoneScoped);

        using Tag = Updated<Component>;
        ECS_ASSERT(has<Component>(target), "Entity should have Component before you can marked it as Updated");
        emplace<Tag>(target);
    }

    template<typename Component, EcsTarget Target>
    ECS_FORCEINLINE void clearUpdateTag(Target target) {
        ECS_PROFILER(ZoneScoped);

        using Tag = Updated<Component>;
        erase<Tag>(target);
    }

    template<typename Component, EcsTarget Target>
    ECS_FORCEINLINE void erase(Target target) {
        ECS_PROFILER(ZoneScoped);

        ECS_ASSERT(m_storages.size() > detail::world::sequenceID<Component>(), "Storage doesn't exist");
        ECS_ASSERT(isAlive(target), "Entity doesn't exist");
        auto* storage = static_cast<Storage<Component>*>(m_storages.at(detail::world::sequenceID<Component>()).get());
        storage->erase(target);
        notify(target);
    }

    template<typename Component>
    requires(!std::is_empty_v<Component>)
    [[nodiscard]] ECS_FORCEINLINE decltype(auto) get(Entity e) noexcept {
        ECS_PROFILER(ZoneScoped);

        ECS_ASSERT(m_storages.size() > detail::world::sequenceID<Component>(), "Storage doesn't exist");
        ECS_ASSERT(isAlive(e), "Entity doesn't exist");
        auto* storage = static_cast<Storage<Component>*>(m_storages.at(detail::world::sequenceID<Component>()).get());
        return storage->get(e);
    }

    template<typename Component>
    requires(!std::is_empty_v<Component>)
    [[nodiscard]] ECS_FORCEINLINE decltype(auto) tryGet(Entity e) noexcept {
        ECS_PROFILER(ZoneScoped);

        ECS_ASSERT(m_storages.size() > detail::world::sequenceID<Component>(), "Storage doesn't exist");
        ECS_ASSERT(isAlive(e), "Entity doesn't exist");
        auto* storage = static_cast<Storage<Component>*>(m_storages.at(detail::world::sequenceID<Component>()).get());
        return storage->tryGet(e);
    }

    template<typename Component>
    [[nodiscard]] const std::vector<Entity>& entities() const noexcept {
        ECS_PROFILER(ZoneScoped);

        ECS_ASSERT(m_storages.size() > detail::world::sequenceID<Component>(), "Storage doesn't exist");
        auto* storage = static_cast<Storage<Component>*>(m_storages.at(detail::world::sequenceID<Component>()).get());
        return storage->entities();
    }

    [[nodiscard]] Entity create() {
        ECS_PROFILER(ZoneScoped);

        Entity entity = 0;

        if (m_free_entities.empty()) [[unlikely]] {
            entity = m_entities.size();
            m_entities.emplace_back(entity);
        } else {
            entity = m_free_entities.back();
            m_free_entities.pop_back();

            auto lower = std::ranges::lower_bound(m_entities, entity);
            m_entities.insert(lower, entity);
        }

        notify(entity);
        return entity;
    }

    void destroy(Entity e) {
        ECS_PROFILER(ZoneScoped);
        m_entities_to_destroy.emplace_back(e);
    }

    void destroy(std::span<const Entity> entities) {
        ECS_PROFILER(ZoneScoped);
        m_entities_to_destroy.insert(m_entities_to_destroy.end(), entities.begin(), entities.end());
    }

    void flush() {
        ECS_PROFILER(ZoneScoped);

        if (m_entities_to_destroy.empty()) {
            return;
        }

        if (m_entities_to_destroy.size() > 1) {
            std::ranges::sort(m_entities_to_destroy);
            const auto [first, last] = std::ranges::unique(m_entities_to_destroy);
            m_entities_to_destroy.erase(first, last);
        }

        for (auto& storage : m_storages) {
            storage->remove(m_entities_to_destroy);
        }

        for (auto entity : m_entities_to_destroy) {
            ECS_ASSERT(isAlive(entity), "Entity doesn't exist");

            if (!m_free_entities.empty() && m_free_entities.back() < entity) {
                m_free_entities.emplace_back(entity);
            } else {
                m_free_entities.emplace_front(entity);
            }

            notify(entity);
        }

        if (m_entities_to_destroy.size() > 1) {
            static std::vector<Entity> result;
            result.reserve(m_entities.size());
            std::ranges::set_difference(m_entities, m_entities_to_destroy, std::back_inserter(result));
            m_entities.swap(result);
            result.clear();
        } else {
            auto pos = std::ranges::lower_bound(m_entities, m_entities_to_destroy.front());
            m_entities.erase(pos);
        }

        m_entities_to_destroy.clear();
    }

    std::vector<std::string> componentsNames(Entity e) {
        ECS_PROFILER(ZoneScoped);

        ECS_RELEASE_ONLY(
          spdlog::warn("Don't use this method in release build. We don't have storage names in release build"));
        ECS_ASSERT(isAlive(e), "Entity doesn't exist");
        std::vector<std::string> names;
        for (const auto& storage : m_storages) {
            if (storage->has(e)) {
                ECS_DEBUG_ONLY(names.emplace_back(storage->name()));
            }
        }

        return names;
    }

    void subscribe(std::function<void(Entity)> func) { m_notify_callback.emplace_back(std::move(func)); }

    void notify(Entity entity) {
        ECS_PROFILER(ZoneScoped);

        for (const auto& func : m_notify_callback) {
            func(entity);
        }
    }

    void notify(std::span<const Entity> entities) {
        ECS_PROFILER(ZoneScoped);

        for (const auto& func : m_notify_callback) {
            for (auto entity : entities) {
                func(entity);
            }
        }
    }

    void optimize(std::size_t storage_id) {
        ECS_PROFILER(ZoneScoped);

        assert(m_storages.size());
        m_storages[storage_id % m_storages.size()]->optimize();
    }


private:
    std::unique_ptr<Registry>                 m_reg;
    std::vector<Entity>                       m_entities;
    std::vector<Entity>                       m_entities_to_destroy;
    std::vector<std::unique_ptr<StorageBase>> m_storages;
    std::vector<std::function<void(Entity)>>  m_notify_callback;
    std::list<Entity>                         m_free_entities;
    std::map<std::string, Component>          m_component_name;
};