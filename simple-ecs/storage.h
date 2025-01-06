#pragma once

#include "simple-ecs/entity.h"
#include "tools/sparse_set.h"

#include <algorithm>
#include <span>

struct StorageBase : SparseSet {
    StorageBase()                                  = default;
    StorageBase(const StorageBase&)                = delete;
    StorageBase(StorageBase&&) noexcept            = default;
    StorageBase& operator=(const StorageBase&)     = delete;
    StorageBase& operator=(StorageBase&&) noexcept = default;
    ~StorageBase() override                        = default;

    const std::vector<Entity>& entities() const noexcept { return m_entities; }
    decltype(auto)             size() const noexcept { return m_entities.size(); }
    decltype(auto)             empty() const noexcept { return m_entities.empty(); }

    virtual void remove(Entity e) = 0;

    ECS_DEBUG_ONLY(IDType id() const noexcept { return m_id; })
    ECS_DEBUG_ONLY(std::string name() const noexcept { return m_string_name; })

    virtual bool optimise() = 0;

protected:
    std::vector<Entity> m_entities;

    ECS_DEBUG_ONLY(std::string m_string_name);
    ECS_DEBUG_ONLY(IDType m_id = 0);
};


template<typename Component>
struct Storage final : StorageBase {
    static_assert(std::is_move_constructible_v<Component>, "Cannot add component which is not move constructible");
    static_assert(std::is_destructible_v<Component>, "Cannot add component which is not destructible");
    static_assert(not std::is_array_v<Component>, "Cannot add array");

    using Callback = std::conditional_t<std::is_empty_v<Component>, //
                                        std::function<void(Entity)>,
                                        std::function<void(Entity, Component&)>>;

    Storage() {
        ECS_DEBUG_ONLY(m_string_name = ct::name<Component>); // NOLINT
        ECS_DEBUG_ONLY(m_id = ct::ID<Component>);            // NOLINT
    }

    Storage(const Storage&)                = delete;
    Storage(Storage&&) noexcept            = default;
    Storage& operator=(const Storage&)     = delete;
    Storage& operator=(Storage&&) noexcept = default;
    ~Storage() override {
        // clean memory to avoid memory leak report
        while (!m_entities.empty()) {
            erase(m_entities.back());
        }
    };

    void remove(Entity e) override { erase(e); }

    void addEmplaceCallback(Callback&& func) { m_on_construct_callbacks.emplace_back(std::forward<Callback>(func)); }
    void addDestroyCallback(Callback&& func) { m_on_destroy_callbacks.emplace_back(std::forward<Callback>(func)); }

    template<typename... Args>
    requires std::is_constructible_v<Component, Args...>
    ECS_FORCEINLINE void emplace(Entity e, Args&&... args) {
        if (SparseSet::emplace(e)) {
            if constexpr (!std::is_empty_v<Component>) {
                m_components.emplace_back(std::forward<Args>(args)...);
            }

            auto lower = std::ranges::lower_bound(m_entities, e);
            m_entities.insert(lower, e);

            for (const auto& function : m_on_construct_callbacks) { // do something after construct
                if constexpr (std::is_empty_v<Component>) {
                    std::invoke(function, e);
                } else {
                    std::invoke(function, e, m_components[m_sparse[e]]);
                }
            }
        }
    }

    ECS_FORCEINLINE void erase(Entity e) {
        if (has(e)) {
            eraseOne(e);

            auto lower = std::ranges::lower_bound(m_entities, e);
            m_entities.erase(lower);
        }
    }

    ECS_FORCEINLINE void erase(std::span<const Entity> ents) {
        if (ents.empty()) {
            return;
        }

        for (const Entity& e : ents) {
            if (has(e)) {
                eraseOne(e);
            }
        }

        static std::vector<Entity> result;
        result.reserve(m_entities.size());
        std::ranges::set_difference(m_entities, ents, std::back_inserter(result));
        m_entities.swap(result);
        result.clear();
    }


    [[nodiscard]] ECS_FORCEINLINE Component& get(Entity e) noexcept
    requires(!std::is_empty_v<Component>)
    {
        assert(has(e) && "Cannot get a component which an entity does not have");
        return m_components[m_sparse[e]];
    }


    [[nodiscard]] ECS_FORCEINLINE Component* tryGet(Entity e) noexcept
    requires(!std::is_empty_v<Component>)
    {
        if (has(e)) {
            return &m_components[m_sparse[e]];
        }
        return nullptr;
    }

    bool optimise() override {
        if constexpr (std::is_empty_v<Component>) {
            return true;
        } else {
            if (m_dense.empty()) {
                return true;
            }

            bool sorted = true;
            // make one sort pass to avoid long block
            for (auto it = std::next(m_dense.begin()); it < m_dense.end(); ++it) {
                if (*std::prev(it) > *it) {
                    auto prev = std::prev(it);
                    std::swap(m_components[m_sparse[*prev]], m_components[m_sparse[*it]]);
                    std::swap(m_sparse[*prev], m_sparse[*it]);
                    std::iter_swap(prev, it);
                    sorted = false;
                }
            }
            return sorted;
        }
    }

private:
    ECS_FORCEINLINE void eraseOne(Entity e) {
        for (const auto& function : m_on_destroy_callbacks) { // do something before remove
            if constexpr (std::is_empty_v<Component>) {
                std::invoke(function, e);
            } else {
                std::invoke(function, e, m_components[m_sparse[e]]);
            }
        }

        if constexpr (!std::is_empty_v<Component>) {
            std::swap(m_components[m_sparse[e]], m_components.back());
            m_components.pop_back();
        }

        SparseSet::erase(e);
    }

private:
    std::vector<Component> m_components;
    std::vector<Callback>  m_on_destroy_callbacks;
    std::vector<Callback>  m_on_construct_callbacks;
};
