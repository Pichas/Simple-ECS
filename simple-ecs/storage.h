#pragma once

#include "simple-ecs/entity.h"
#include "simple-ecs/tools/void_callback.h"
#include "tools/sparse_set.h"

#include <algorithm>
#include <cstring>
#include <span>


struct Storage final : SparseSet {
    Storage()                              = default;
    Storage(const Storage&)                = delete;
    Storage(Storage&&) noexcept            = default;
    Storage& operator=(const Storage&)     = delete;
    Storage& operator=(Storage&&) noexcept = default;
    ~Storage() override {
        // clean memory to avoid memory leak report
        while (!m_entities.empty()) {
            remove(m_entities.back());
        }
    };

    const std::vector<Entity>& entities() const noexcept { return m_entities; }

    ECS_DEBUG_ONLY(IDType id() const noexcept { return m_id; })
    ECS_DEBUG_ONLY(std::string name() const noexcept { return m_string_name; })

    template<typename Component>
    void setType() {
        static_assert(std::is_move_constructible_v<Component>, "Cannot add component which is not move constructible");
        static_assert(std::is_destructible_v<Component>, "Cannot add component which is not destructible");
        static_assert(not std::is_array_v<Component>, "Cannot add array");

        assert(m_components_memory.empty() && "storage already in use");

        ECS_DEBUG_ONLY(m_string_name = ct::name<Component>);
        ECS_DEBUG_ONLY(m_id = ct::ID<Component>);

        m_component_size = sizeof(Component);
        m_components_memory.reserve(1024ull * 1024 * m_component_size);

        // don't use [this] here, because Storage will be moved in the memory
        m_remove_callback = +[](Storage* ptr, Entity e) { ptr->erase<Component>(e); };
    }

    void remove(Entity e) {
        // used when we don't know the type of the component
        assert(m_remove_callback && "you need to call `setType` to use this function");
        m_remove_callback(this, e);
    }

    template<typename Component, typename Callback, typename... Args>
    void addDestroyCallback(Callback&& func) {
        ECS_DEBUG_ONLY(assert(m_id == ct::ID<Component>));
        m_on_destroy_callbacks.emplace_back(voidCallbackBuilder<Args...>(std::forward<Callback>(func)));
    }

    template<typename Component, typename Callback, typename... Args>
    void addConstructCallback(Callback&& func) {
        ECS_DEBUG_ONLY(assert(m_id == ct::ID<Component>));
        m_on_construct_callbacks.emplace_back(voidCallbackBuilder<Args...>(std::forward<Callback>(func)));
    }

    template<typename Component, typename... Args>
    requires std::is_constructible_v<Component, Args...>
    ECS_FORCEINLINE void emplace(Entity e, Args&&... args) {
        ECS_DEBUG_ONLY(assert(m_id == ct::ID<Component>));

        if (SparseSet::emplace(e)) {
            if constexpr (!std::is_empty_v<Component>) {
                m_components_memory.resize(m_components_memory.size() + m_component_size);
                std::byte* ptr      = &m_components_memory[m_components_memory.size() - m_component_size];
                Component* comp_ptr = std::launder(reinterpret_cast<Component*>(ptr));
                std::construct_at(comp_ptr, std::forward<Args>(args)...);
            }

            auto lower = std::ranges::lower_bound(m_entities, e);
            m_entities.insert(lower, e);

            for (const auto& function : m_on_construct_callbacks) { // do something after construct
                if constexpr (std::is_empty_v<Component>) {
                    (*function)(e);
                } else {
                    auto* comp = reinterpret_cast<Component*>(&m_components_memory[m_sparse[e] * m_component_size]);
                    (*function)(e, *std::launder(comp));
                }
            }
        }
    }

    template<typename Component>
    ECS_FORCEINLINE void erase(Entity e) {
        ECS_DEBUG_ONLY(assert(m_id == ct::ID<Component>));

        if (has(e)) {
            eraseOne<Component>(e);

            auto lower = std::ranges::lower_bound(m_entities, e);
            m_entities.erase(lower);
        }
    }

    template<typename Component>
    ECS_FORCEINLINE void erase(std::span<const Entity> ents) {
        ECS_DEBUG_ONLY(assert(m_id == ct::ID<Component>));

        if (ents.empty()) {
            return;
        }

        for (const Entity& e : ents) {
            if (has(e)) {
                eraseOne<Component>(e);
            }
        }

        std::vector<Entity> result;
        result.reserve(m_entities.size());
        std::ranges::set_difference(m_entities, ents, std::back_inserter(result));
        m_entities = std::move(result);
    }

    template<typename Component>
    requires(!std::is_empty_v<Component>)
    [[nodiscard]] ECS_FORCEINLINE Component& get(Entity e) noexcept {
        ECS_DEBUG_ONLY(assert(m_id == ct::ID<Component>));

        assert(has(e) && "Cannot get a component which an entity does not have");
        auto* ptr = reinterpret_cast<Component*>(&m_components_memory[m_sparse[e] * m_component_size]);
        return *std::launder(ptr);
    }

    template<typename Component>
    requires(!std::is_empty_v<Component>)
    [[nodiscard]] ECS_FORCEINLINE Component* tryGet(Entity e) noexcept {
        ECS_DEBUG_ONLY(assert(m_id == ct::ID<Component>));

        if (has(e)) {
            auto* ptr = reinterpret_cast<Component*>(&m_components_memory[m_sparse[e] * m_component_size]);
            return std::launder(ptr);
        }
        return nullptr;
    }

private:
    template<typename Component>
    ECS_FORCEINLINE void eraseOne(Entity e) {
        ECS_DEBUG_ONLY(assert(m_id == ct::ID<Component>));

        for (const auto& function : m_on_destroy_callbacks) { // do something before remove
            if constexpr (std::is_empty_v<Component>) {
                (*function)(e);
            } else {
                auto* comp = reinterpret_cast<Component*>(&m_components_memory[m_sparse[e] * m_component_size]);
                (*function)(e, *std::launder(comp));
            }
        }

        if constexpr (!std::is_empty_v<Component>) {
            auto* comp = reinterpret_cast<Component*>(&m_components_memory[m_sparse[e] * m_component_size]);
            std::destroy_at(std::launder(comp));

            std::byte* last = &m_components_memory[m_components_memory.size() - m_component_size];
            if (static_cast<void*>(comp) != static_cast<void*>(last)) {
                std::memcpy(comp, last, m_component_size);
            }
            m_components_memory.resize(m_components_memory.size() - m_component_size);
        }

        SparseSet::erase(e);
    }

private:
    std::size_t                                m_component_size = 0;
    std::vector<Entity>                        m_entities;
    std::vector<std::byte>                     m_components_memory;
    std::vector<std::unique_ptr<VoidCallback>> m_on_destroy_callbacks;
    std::vector<std::unique_ptr<VoidCallback>> m_on_construct_callbacks;
    void (*m_remove_callback)(Storage*, Entity) = nullptr;

    ECS_DEBUG_ONLY(std::string m_string_name);
    ECS_DEBUG_ONLY(IDType m_id = 0);
};
