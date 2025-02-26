#include "simple-ecs/serializer.h"

#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>
#include <algorithm>
#include <cassert>
#include <ranges>


namespace detail::serializer
{

static bool checkSaveLoadCallbacks(
  const std::unordered_map<Component, std::function<void(Entity, ::serializer::Output&)>>& save_functions,
  const std::unordered_map<Component, std::function<void(Entity, ::serializer::Input&)>>&  load_functions) {
    if (save_functions.size() != load_functions.size()) {
        return false;
    }

    auto keys = std::views::keys(save_functions);
    auto it   = std::ranges::find_if(keys, [&load_functions](Component id) { return !load_functions.contains(id); });

    return it == keys.end();
}

} // namespace detail::serializer

Serializer::Serializer(World& world) : m_world(world) {};

serializer::Output Serializer::save() {
    ECS_PROFILER(ZoneScoped);

    assert(detail::serializer::checkSaveLoadCallbacks(m_save_functions, m_load_functions) &&
           "For each save function, you should have one load function");

    spdlog::stopwatch  sw;
    serializer::Output data;

    for (auto entity : m_world.entities()) {
        auto&& id = serializer::serialize(ct::ID<Component>);
        std::ranges::copy(id, std::back_inserter(data));
        for (const auto& func : std::views::values(m_save_functions)) {
            std::invoke(func, entity, data);
        }
    }

    spdlog::info("Saved {:.3}", sw);
    return data;
}


void Serializer::load(std::span<const serializer::Data> data) {
    ECS_PROFILER(ZoneScoped);

    assert(detail::serializer::checkSaveLoadCallbacks(m_save_functions, m_load_functions) &&
           "For each save function, you should have one load function");

    if (data.empty()) {
        return;
    }

    spdlog::stopwatch sw;

    const auto* ptr = data.data();

    Entity entity = 0;
    do {
        auto comp_id = *std::launder(reinterpret_cast<const IDType*>(ptr));
        ptr += sizeof(IDType);

        if (ct::ID<Entity> == comp_id) {
            entity = m_world.create();
            continue;
        }

        auto it = m_load_functions.find(comp_id);
        if (it != m_load_functions.end()) {
            std::invoke(it->second, entity, ptr);
        }
    } while (ptr - data.data() < data.size()); // NOLINT

    spdlog::info("Loaded {:.3}", sw);
}
