#pragma once

#include "simple-ecs/entity_debug.h"
#include "simple-ecs/registry.h"
#include "simple-ecs/serializer.h"
#include "simple-ecs/world.h"

template<typename... Components>
struct ComponentRegistrant {
    ComponentRegistrant(World& world) : m_world(world), m_registry(*m_world.getRegistry()) {}

    auto& createStorage() {
        (m_world.createStorage<Components>(), ...);
        return *this;
    }

    auto& addSerialize() {
        auto& serializer = m_registry.serializer();
        (serializer.registerType<Components>(), ...);
        return *this;
    }

    template<typename Callback>
    requires(sizeof...(Components) == 1)
    auto& setSaveFunc(Callback&& f) {
        auto& serializer = m_registry.serializer();
        (serializer.registerCustomSaver<Components>(std::forward<Callback>(f)), ...);

        return *this;
    }

    template<typename Callback>
    requires(sizeof...(Components) == 1)
    auto& setLoadFunc(Callback&& f) {
        auto& serializer = m_registry.serializer();
        (serializer.registerCustomLoader<Components>(std::forward<Callback>(f)), ...);
        return *this;
    }

    template<typename Callback>
    auto& addDestroyCallback(Callback&& f) {
        (m_world.addDestroyCallback<Components>(std::forward<Callback>(f)), ...);
        return *this;
    }

    template<typename Callback>
    auto& addConstructCallback(Callback&& f) {
        (m_world.addConstructCallback<Components>(std::forward<Callback>(f)), ...);
        return *this;
    }

    auto& addDebuger() {
        if (auto* debug = m_registry.getSystem<EntityDebugSystem>(); debug) {
            (debug->registerDebugComponent<Components>(), ...);
        } else {
            spdlog::warn("Can't find EntityDebugSystem");
        }
        return *this;
    }

    template<string_literal Title, typename Callback>
    requires(sizeof...(Components) == 1)
    auto& addCustomDebuger(const Callback& f) {
        if (auto* debug = m_registry.getSystem<EntityDebugSystem>(); debug) {
            (debug->registerDebugComponent<Components, Title>(f), ...);
        } else {
            spdlog::warn("Can't find EntityDebugSystem");
        }
        return *this;
    }


    auto& addCreateFunc() {
        if (auto* debug = m_registry.getSystem<EntityDebugSystem>(); debug) {
            (debug->registerAddComponent<Components>(), ...);
        } else {
            spdlog::warn("Can't find EntityDebugSystem");
        }
        return *this;
    }

    template<string_literal Title, typename Callback>
    requires(sizeof...(Components) == 1)
    auto& addCustomCreateFunc(const Callback& f) {
        if (auto* debug = m_registry.getSystem<EntityDebugSystem>(); debug) {
            (debug->registerAddComponent<Components, Title>(f), ...);
        } else {
            spdlog::warn("Can't find EntityDebugSystem");
        }
        return *this;
    }

private:
    World&    m_world;
    Registry& m_registry;
};
