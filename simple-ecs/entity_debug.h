#pragma once

#include "simple-ecs/base_system.h"
#include "simple-ecs/observer.h"
#include "simple-ecs/world.h"


#ifdef ECS_ENABLE_IMGUI
#include <imgui.h>
#endif

#ifdef ECS_ENABLE_IMGUI
// utility structure for realtime plot
struct ScrollingBuffer : public NoCopyNoMove {
    int              max_size{};
    int              offset{0};
    ImVector<ImVec2> data;

    ScrollingBuffer(int size = 600 * 60) : max_size(size) {
        assert(size);
        data.reserve(max_size);
        data.push_back(ImVec2(0, 0));
    }

    void addPoint(float x, float y) {
        if (data.size() < max_size) {
            data.push_back(ImVec2(x, y));
        } else {
            data[offset] = ImVec2(x, y);
            offset       = (offset + 1) % max_size;
        }
    }
};
#endif

template<typename Component>
requires(std::is_empty_v<Component>)
void debug([[maybe_unused]] World& w, [[maybe_unused]] Entity e) {
#ifdef ECS_ENABLE_IMGUI
    if (w.has<Component>(e)) {
        ImGui::PushID(ct::NAME<Component>.data());
        ImGui::Text("%s", std::string(ct::NAME<Component>).c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove")) {
            w.erase<Component>(e);
        }
        ImGui::PopID();
    }
#endif
}

// create a new specialization to debug component
template<typename Component>
requires(!std::is_empty_v<Component>)
void debug(Component& component, Entity entity, bool& toMarkUpdated);


struct Name {
    std::string name;
};

template<>
inline void debug([[maybe_unused]] Name& component, Entity /*entity*/, bool& /*toMarkUpdated*/) {
#ifdef ECS_ENABLE_IMGUI
    ImGui::Text("%s", component.name.data());
#endif
}

struct EntityDebugSystem final : BaseSystem {
    EntityDebugSystem(World& world);
    ~EntityDebugSystem() override = default;
    void setup(Registry& reg);

    void showRegisteredComponents();
    void showRegisteredFunctions();
    void showEntityListUI();

    // debug information for tags
    template<typename Component>
    requires(std::is_empty_v<Component>)
    void registerDebugComponent() {
        ECS_PROFILER(ZoneScoped);

        m_debug_callbacks.emplace_back([&world = m_world](Entity e) {
            ECS_PROFILER(ZoneScoped);
            debug<Component>(world, e);
        });
    }

    // debug function from T
    template<typename Component>
    requires(!std::is_empty_v<Component> &&
             std::is_invocable_r_v<void, decltype(&debug<Component>), Component&, Entity, bool&>)
    void registerDebugComponent() {
        ECS_PROFILER(ZoneScoped);

        m_debug_callbacks.emplace_back([&world = m_world]([[maybe_unused]] Entity e) {
            ECS_PROFILER(ZoneScoped);

#ifdef ECS_ENABLE_IMGUI
            Component* c     = world.tryGet<Component>(e);
            bool       state = true;

            if (c && ImGui::CollapsingHeader(std::string(ct::NAME<Component>).c_str(), &state)) {
                ImGui::PushID(c);
                ImGui::Indent();

                bool update = false;
                debug<Component>(*c, e, update);
                if (update) {
                    world.emplace<Updated<Component>>(e);
                }

                ImGui::Separator();
                ImGui::Unindent();
                ImGui::PopID();
            }

            if (!state) { //-V547
                world.erase<Component>(e);
            }
#endif
        });
    }


    template<typename Component, string_literal Title, typename Callback>
    requires(!std::is_empty_v<Component> && std::is_invocable_r_v<void, Callback, Entity, Component*, bool&>)
    void registerDebugComponent(Callback&& callback) { // NOLINT
        ECS_PROFILER(ZoneScoped);

        m_debug_callbacks.emplace_back(
          [&world = m_world, callback = std::forward<Callback>(callback)]([[maybe_unused]] Entity e) {
              ECS_PROFILER(ZoneScoped);

#ifdef ECS_ENABLE_IMGUI
              Component* c     = world.tryGet<Component>(e);
              bool       state = true;

              if (c && ImGui::CollapsingHeader(Title.value, &state)) {
                  ImGui::PushID(c);
                  ImGui::Indent();

                  bool update = false;
                  callback(e, c, update);
                  if (update) {
                      world.emplace<Updated<Component>>(e);
                  }

                  ImGui::Separator();
                  ImGui::Unindent();
                  ImGui::PopID();
              }

              if (!state) { //-V547
                  world.erase<Component>(e);
              }
#endif
          });
    }

    // create a Component with custom function
    template<typename Component, string_literal Title, typename Callback>
    void registerAddComponent(Callback&& f) {
        ECS_PROFILER(ZoneScoped);

        auto [_, was_added] = m_create_callbacks.try_emplace(
          Title, [&world = m_world, f = std::forward<Callback>(f)](Entity e) { std::invoke(f, world, e); });
        assert(was_added);
    }

    // create a Component with default ctor
    template<typename Component>
    void registerAddComponent() {
        ECS_PROFILER(ZoneScoped);

        auto [_, was_added] = m_create_callbacks.try_emplace(
          std::string(ct::NAME<Component>), [&world = m_world](Entity e) { world.emplace<Component>(e); });
        assert(was_added);
    }

private:
    void trackEntitiesCount(OBSERVER(RunEveryFrame) /*unused*/);

private:
    bool showEntityInfoUI(Entity);
    void entityHistory();

private:
    World&                                                       m_world;
    std::vector<std::function<void(Entity)>>                     m_debug_callbacks;
    std::unordered_map<std::string, std::function<void(Entity)>> m_create_callbacks;

#ifdef ECS_ENABLE_IMGUI
    ScrollingBuffer m_entities_history;
#endif
    float m_time                  = 0;
    bool  m_show_entities_history = false;
};
