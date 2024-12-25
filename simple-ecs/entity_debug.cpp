#include <algorithm>

#include "simple-ecs/entity_debug.h"
#include "simple-ecs/registrant.h"
#include "simple-ecs/registry.h"

#ifdef ECS_ENABLE_IMGUI
#include <implot.h>
#endif

EntityDebugSystem::EntityDebugSystem(World& world) : m_world(world) {}

void EntityDebugSystem::setup(Registry& reg) { // NOLINT
    ECS_NOT_FINAL_ONLY(ECS_REG_FUNC(reg, EntityDebugSystem::trackEntitiesCount));

    ComponentRegistrant<Name>(m_world)
      .createStorage()
      .addDebuger()
      .addCreateFunc()
      .addDestroyCallback([](Entity e, Name& c) { spdlog::debug("Entity {} ({}) was removed", c.name.c_str(), e); })
      .addConstructCallback([](Entity e, Name& c) { spdlog::debug("Entity {} ({}) was created", c.name.c_str(), e); })
      .setSaveFunc([](const Name& comp) {
          std::vector<std::uint8_t>        temp;
          const std::vector<std::uint8_t>& size = serializer::serialize(comp.name.size());
          std::ranges::copy(size, std::back_inserter(temp));
          // BUG: throws iterator error when use comp.name directly
          std::ranges::copy(std::string_view(comp.name), std::back_inserter(temp));
          return temp;
      })
      .setLoadFunc([](const std::uint8_t*& data) {
          auto        size = serializer::deserialize<std::string::size_type>(data);
          std::string temp(size, 0);
          std::memcpy(temp.data(), data, size);
          data += size;
          return Name{temp};
      });
}

void EntityDebugSystem::trackEntitiesCount(OBSERVER(RunEveryFrame) /*unused*/) {
#ifdef ECS_ENABLE_IMGUI
    static float time_tick = 0;

    time_tick += ImGui::GetIO().DeltaTime;
    if (time_tick > (1. / 60.)) {
        m_time += time_tick;
        time_tick = 0;
        m_entities_history.addPoint(m_time, m_world.size());
    }
#endif
}

void EntityDebugSystem::showRegisteredComponents() {
#ifdef ECS_ENABLE_IMGUI
    static bool show = false;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Debug")) {
            ImGui::MenuItem("All components", nullptr, &show);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (show && ImGui::Begin("Component list", &show, ImGuiWindowFlags_NoCollapse)) {
        constexpr ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;

        if (ImGui::BeginTable("components", 2, flags)) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 0.0f);
            ImGui::TableHeadersRow();

            for (const auto& [name, id] : m_world.registeredComponentNames()) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", name.c_str());

                ImGui::TableNextColumn();
                ImGui::Text("%08X", id);
            }
            ImGui::EndTable();
        }
        ImGui::End();
    }
#endif
}

void EntityDebugSystem::showRegisteredFunctions() {
#ifdef ECS_ENABLE_IMGUI
    static bool show = false;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Debug")) {
            ImGui::MenuItem("All functions", nullptr, &show);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (show && ImGui::Begin("Function list", &show, ImGuiWindowFlags_NoCollapse)) {
        for (const auto& [time, name] : m_world.getRegistry()->getRegisteredFunctionsInfo()) {
            ImGui::Text("%.7f s - %s", time, name.data());
        }
        ImGui::End();
    }
#endif
}


void EntityDebugSystem::showEntityListUI() {
#ifdef ECS_ENABLE_IMGUI
    static bool                                                  show = false;
    static std::unordered_map<Entity, std::function<bool(void)>> show_entity_info;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Debug")) {
            ImGui::MenuItem("Entities", nullptr, &show);
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (show && ImGui::Begin("Entity list", &show, ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("Total %zu", m_world.size());
        ImGui::SameLine();
        if (ImGui::SmallButton("History")) {
            m_show_entities_history = true;
        }

        ImGui::Separator();

        for (const auto& e : m_world.entities()) {
            ImGui::PushID(static_cast<int>(e));

            if (ImGui::SmallButton("X")) {
                m_world.destroy(e);
            }

            ImGui::SameLine();
            if (ImGui::SmallButton("View")) {
                show_entity_info.emplace(e, std::bind_front(&EntityDebugSystem::showEntityInfoUI, this, e));
            }

            ImGui::SameLine();
            auto* name = m_world.tryGet<Name>(e);
            if (name) {
                ImGui::Text("%s (%u)", name->name.data(), e);
            } else {
                ImGui::Text("Entity: %u", e);
            }

            ImGui::PopID();
        }

        // show info for selected entity
        for (auto it = show_entity_info.begin(); it != show_entity_info.end();) {
            it = (it->second)() ? show_entity_info.erase(it) : ++it;
        }

        if (ImGui::Button("+Entity")) {
            (void)m_world.create();
        }

        if (m_show_entities_history) {
            entityHistory();
        }

        ImGui::End();
    } else {
        show_entity_info.clear();
        m_show_entities_history = false;
    }
#endif
}

bool EntityDebugSystem::showEntityInfoUI(Entity e) { // NOLINT
#ifdef ECS_ENABLE_IMGUI
    bool show = true;

    auto*       name   = m_world.tryGet<Name>(e);
    std::string header = (name ? name->name : "Entity") + " (" + std::to_string(e) + ")";

    if (show && ImGui::Begin(header.c_str(), &show, ImGuiWindowFlags_NoCollapse)) {
        for (const auto& debug : m_debug_callbacks) {
            debug(e);
        }

        if (ImGui::Button("+Comp")) {
            ImGui::OpenPopup("components");
        }

        if (ImGui::BeginPopup("components")) {
            ImGui::SeparatorText("Components");
            for (const auto& [name, create] : m_create_callbacks) {
                if (ImGui::Selectable(name.data())) {
                    create(e);
                }
            }
            ImGui::EndPopup();
        }
        ImGui::End();
    }

    return !show;
#else
    return false;
#endif
}

void EntityDebugSystem::entityHistory() {
#ifdef ECS_ENABLE_IMGUI
    if (m_show_entities_history &&
        ImGui::Begin("Entity count", &m_show_entities_history, ImGuiWindowFlags_NoCollapse)) {
        static float history = 60.0f;
        double       size    = m_world.size();

        ImGui::SliderFloat("History", &history, 1, 600, "%.0f s");
        if (ImPlot::BeginPlot("##Scrolling", ImVec2(-1, -1))) {
            ImPlot::SetupAxes(nullptr, nullptr, 0, ImPlotAxisFlags_Opposite | ImPlotAxisFlags_AutoFit);
            ImPlot::SetupAxisLimits(ImAxis_X1, m_time - history, m_time, ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0, size);
            ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.5f);
            ImPlot::PlotShaded("Entities",
                               &m_entities_history.data[0].x,
                               &m_entities_history.data[0].y,
                               m_entities_history.data.size(),
                               0,
                               0,
                               m_entities_history.offset,
                               2 * sizeof(float));

            ImPlot::TagY(size, ImVec4(1, 0, 0, 1), "%.0f", size);
            ImPlot::DragLineY(0, &size, ImVec4(1, 0, 0, 1), 1, ImPlotDragToolFlags_NoFit);

            ImPlot::EndPlot();
        }

        ImGui::End();
    }
#endif
}
