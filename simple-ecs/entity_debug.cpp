#include <algorithm>

#include "simple-ecs/entity_debug.h"
#include "simple-ecs/registrant.h"
#include "simple-ecs/registry.h"

#ifdef ECS_ENABLE_IMGUI
#include <implot.h>
#endif

EntityDebugSystem::EntityDebugSystem(World& world) : m_world(world) {}

void EntityDebugSystem::setup([[maybe_unused]] Registry& reg) {
    ECS_PROFILER(ZoneScoped);

    ECS_NOT_FINAL_ONLY(ECS_REG_FUNC(reg, EntityDebugSystem::trackEntitiesCount));

    ComponentRegistrant<Name>(m_world)
      .createStorage()
      .addDebuger()
      .addCreateFunc()
      .addEmplaceCallback([](Entity e, Name& c) { spdlog::debug("Entity {} ({}) was created", c.name.c_str(), e); })
      .addDestroyCallback([](Entity e, Name& c) { spdlog::debug("Entity {} ({}) was removed", c.name.c_str(), e); })
      .setSaveFunc([](const Name& comp) {
          serializer::Output temp;
          auto&&             size = serializer::serialize(comp.name.size());
          std::ranges::copy(size, std::back_inserter(temp));
          std::ranges::copy(comp.name, std::back_inserter(temp));
          return temp;
      })
      .setLoadFunc([](serializer::Input& data) -> Name {
          auto&&      size = serializer::deserialize<std::string::size_type>(data);
          std::string temp(size, 0);
          std::memcpy(temp.data(), data, size);
          data += size;
          return {std::move(temp)};
      });
}

void EntityDebugSystem::trackEntitiesCount(OBSERVER(RunEveryFrame) /*unused*/) {
    ECS_PROFILER(ZoneScoped);

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

void EntityDebugSystem::showRegisteredComponents([[maybe_unused]] bool& show) {
    ECS_PROFILER(ZoneScoped);

#ifdef ECS_ENABLE_IMGUI
    if (!show) {
        return;
    }

    if (ImGui::Begin("Component list", &show, ImGuiWindowFlags_NoCollapse)) {
        constexpr ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;

        if (ImGui::BeginTable("components", 2, flags)) {
            ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 0.0f);
            ImGui::TableHeadersRow();

            ImGuiListClipper clipper;
            const auto&      components = m_world.registeredComponentNames();
            clipper.Begin(components.size());

            while (clipper.Step()) {
                for (int row_n = clipper.DisplayStart; row_n < clipper.DisplayEnd; row_n++) {
                    auto it = components.begin();
                    std::advance(it, row_n);
                    const auto& [name, id] = *it;
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", name.c_str());

                    ImGui::TableNextColumn();
                    ImGui::Text("%08X", id);
                }
            }
            ImGui::EndTable();
        }
    }

    ImGui::End();
#endif
}

void EntityDebugSystem::showRegisteredFunctions([[maybe_unused]] bool& show) {
    ECS_PROFILER(ZoneScoped);

#ifdef ECS_ENABLE_IMGUI
    if (!show) {
        return;
    }

    if (ImGui::Begin("Function list", &show, ImGuiWindowFlags_NoCollapse)) {
        constexpr ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;

        if (ImGui::BeginTable("functions", 2, flags)) {
            ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("ms", ImGuiTableColumnFlags_WidthFixed, 0.0f);
            ImGui::TableHeadersRow();

            ImGuiListClipper clipper;
            const auto&      components = m_world.getRegistry()->getRegisteredFunctionsInfo();
            clipper.Begin(components.size());

            while (clipper.Step()) {
                for (int row_n = clipper.DisplayStart; row_n < clipper.DisplayEnd; row_n++) {
                    auto it = components.begin();
                    std::advance(it, row_n);
                    const auto& [time, name] = *it;
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Text("%s", name.data()); // NOLINT

                    ImGui::TableNextColumn();
                    auto text = std::format("{:08.04f}", time * 1000);
                    ImGui::Text("%s ", text.c_str()); // NOLINT
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
#endif
}


void EntityDebugSystem::showEntityListUI([[maybe_unused]] bool& show) {
    ECS_PROFILER(ZoneScoped);

#ifdef ECS_ENABLE_IMGUI
    static std::unordered_map<Entity, std::function<bool(void)>> show_entity_info;

    if (!show) {
        return;
    }

    if (ImGui::Begin("Entity list", &show, ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("Total %zu", m_world.size());
        ImGui::SameLine();
        if (ImGui::SmallButton("History")) {
            m_show_entities_history = true;
        }

        ImGui::Separator();

        if (ImGui::Button("+Entity")) {
            std::ignore = m_world.create();
        }

        constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;
        if (ImGui::BeginTable("Entities", 3, flags)) {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Button", ImGuiTableColumnFlags_WidthFixed, 0.0f);
            ImGui::TableSetupColumn("Destroy", ImGuiTableColumnFlags_WidthFixed, 0.0f);

            const auto&      entities = m_world.entities();
            ImGuiListClipper clipper;
            clipper.Begin(entities.size());

            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                    ImGui::TableNextRow();

                    auto e = entities[row];
                    ImGui::PushID(static_cast<int>(e));

                    ImGui::TableNextColumn();
                    auto ent_name = ""_t;
                    if (auto* name = m_world.tryGet<Name>(e)) {
                        *ent_name = std::format("{} ({})", name->name, e);
                    } else {
                        *ent_name = std::format("Entity: {}", e);
                    }

                    ImGui::Selectable(ent_name->c_str(),
                                      false,
                                      ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap);


                    ImGui::TableNextColumn();
                    if (ImGui::SmallButton("View")) {
                        show_entity_info.emplace(e, std::bind_front(&EntityDebugSystem::showEntityInfoUI, this, e));
                    }

                    ImGui::TableNextColumn();
                    if (ImGui::SmallButton("X")) {
                        m_world.destroy(e);
                    }

                    ImGui::PopID();
                }
            }
            ImGui::EndTable();
        }

        // show info for selected entity
        for (auto it = show_entity_info.begin(); it != show_entity_info.end();) {
            it = std::invoke(it->second) ? show_entity_info.erase(it) : ++it;
        }


        if (m_show_entities_history) {
            entityHistory();
        }
    } else {
        show_entity_info.clear();
        m_show_entities_history = false;
    }
    ImGui::End();

#endif
}

bool EntityDebugSystem::showEntityInfoUI([[maybe_unused]] Entity e) {
    ECS_PROFILER(ZoneScoped);

#ifdef ECS_ENABLE_IMGUI
    bool show = true;

    auto*       name   = m_world.tryGet<Name>(e);
    std::string header = (name ? name->name : "Entity") + " (" + std::to_string(e) + ")";

    if (ImGui::Begin(header.c_str(), &show, ImGuiWindowFlags_NoCollapse)) {
        for (const auto& func : m_debug_callbacks) {
            func(e);
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
    }
    ImGui::End();

    return !show;
#else
    return false;
#endif
}

void EntityDebugSystem::entityHistory() {
    ECS_PROFILER(ZoneScoped);

#ifdef ECS_ENABLE_IMGUI
    if (!m_show_entities_history) {
        return;
    }

    if (ImGui::Begin("Entity count", &m_show_entities_history, ImGuiWindowFlags_NoCollapse)) {
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
    }

    ImGui::End();
#endif
}
