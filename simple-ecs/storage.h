#pragma once

#include "simple-ecs/entity.h"
#include "simple-ecs/tools/void_callback.h"

#include <span>


struct Storage final : SparseSet {
    Storage()                              = default;
    Storage(const Storage&)                = delete;
    Storage(Storage&&) noexcept            = default;
    Storage& operator=(const Storage&)     = delete;
    Storage& operator=(Storage&&) noexcept = delete;
    ~Storage() override {
        // clean memory to avoid memory leak report
        while (!m_ents.empty()) {
            remove(m_ents.back());
        }
    };

    const std::vector<Entity>& entities() const noexcept { return m_ents; }

    ECS_DEBUG_ONLY(IDType id() const noexcept { return m_id; })
    ECS_DEBUG_ONLY(std::string name() const noexcept { return m_string_name; })

    template<typename Component>
    void setType() {
        assert(m_components_memory.empty() && "storage already in use");

        ECS_DEBUG_ONLY(m_string_name = S_NAME<Component>);
        ECS_DEBUG_ONLY(m_id = ID<Component>);

        m_component_size = sizeof(Component);

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
        ECS_DEBUG_ONLY(assert(m_id == ID<Component>));
        m_on_destroy_callbacks.emplace_back(voidCallbackBuilder<Args...>(std::forward<Callback>(func)));
    }

    template<typename Component, typename Callback, typename... Args>
    void addConstructCallback(Callback&& func) {
        ECS_DEBUG_ONLY(assert(m_id == ID<Component>));
        m_on_construct_callbacks.emplace_back(voidCallbackBuilder<Args...>(std::forward<Callback>(func)));
    }

    template<typename Component, typename... Args>
    requires std::is_constructible_v<Component, Args...>
    ECS_FORCEINLINE void emplace(Entity e, Args&&... args) {
        ECS_DEBUG_ONLY(assert(m_id == ID<Component>));

        if (SparseSet::emplace(e)) {
            if constexpr (!std::is_empty_v<Component>) {
                m_components_memory.resize(m_components_memory.size() + m_component_size);
                std::byte* ptr = &m_components_memory[m_components_memory.size() - m_component_size];
                new (ptr) std::decay_t<Component>(std::forward<Args>(args)...);
            }

            auto lower = std::lower_bound(m_ents.begin(), m_ents.end(), e);
            m_ents.insert(lower, e);

            for (const auto& function : m_on_construct_callbacks) { // do something after construct
                if constexpr (std::is_empty_v<Component>) {
                    (*function)(e);
                } else {
                    auto* comp = reinterpret_cast<Component*>(&m_components_memory[m_sparse[e] * m_component_size]);
                    (*function)(e, *comp);
                }
            }
        }
    }

    template<typename Component>
    ECS_FORCEINLINE void erase(Entity e) {
        ECS_DEBUG_ONLY(assert(m_id == ID<Component>));

        if (has(e)) {
            eraseOne<Component>(e);

            auto lower = std::lower_bound(m_ents.begin(), m_ents.end(), e);
            m_ents.erase(lower);
        }
    }

    template<typename Component>
    ECS_FORCEINLINE void erase(std::span<const Entity> ents) {
        ECS_DEBUG_ONLY(assert(m_id == ID<Component>));

        if (ents.empty()) {
            return;
        }

        for (const Entity& e : ents) {
            if (has(e)) {
                eraseOne<Component>(e);
            }
        }

        auto it = std::set_difference(m_ents.begin(), m_ents.end(), ents.begin(), ents.end(), m_ents.begin());
        m_ents.erase(it, m_ents.end());
    }

    template<typename Component>
    requires(!std::is_empty_v<Component>)
    [[nodiscard]] ECS_FORCEINLINE Component& get(Entity e) noexcept {
        ECS_DEBUG_ONLY(assert(m_id == ID<Component>));

        assert(has(e) && "Cannot get a component which an entity does not have");
        return *reinterpret_cast<Component*>(&m_components_memory[m_sparse[e] * m_component_size]);
    }

    template<typename Component>
    requires(!std::is_empty_v<Component>)
    [[nodiscard]] ECS_FORCEINLINE Component* tryGet(Entity e) noexcept {
        ECS_DEBUG_ONLY(assert(m_id == ID<Component>));

        if (has(e)) {
            return reinterpret_cast<Component*>(&m_components_memory[m_sparse[e] * m_component_size]);
        }
        return nullptr;
    }

private:
    template<typename Component>
    ECS_FORCEINLINE void eraseOne(Entity e) {
        ECS_DEBUG_ONLY(assert(m_id == ID<Component>));

        for (const auto& function : m_on_destroy_callbacks) { // do something before remove
            if constexpr (std::is_empty_v<Component>) {
                (*function)(e);
            } else {
                auto* comp = reinterpret_cast<Component*>(&m_components_memory[m_sparse[e] * m_component_size]);
                (*function)(e, *comp);
            }
        }

        if constexpr (!std::is_empty_v<Component>) {
            auto* comp = reinterpret_cast<Component*>(&m_components_memory[m_sparse[e] * m_component_size]);
            comp->~Component();

            std::byte* last = &m_components_memory[m_components_memory.size() - m_component_size];
            std::memcpy(comp, last, m_component_size);
            m_components_memory.resize(m_components_memory.size() - m_component_size);
        }

        SparseSet::erase(e);
    }

private:
    std::size_t                                m_component_size = 0;
    std::vector<Entity>                        m_ents;
    std::vector<std::byte>                     m_components_memory;
    std::vector<std::unique_ptr<VoidCallback>> m_on_destroy_callbacks;
    std::vector<std::unique_ptr<VoidCallback>> m_on_construct_callbacks;
    void (*m_remove_callback)(Storage*, Entity) = nullptr;

    ECS_DEBUG_ONLY(std::string m_string_name);
    ECS_DEBUG_ONLY(IDType m_id = 0);
};
