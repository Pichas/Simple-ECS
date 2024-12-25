#include "simple-ecs/serializer.h"

#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>
#include <algorithm>
#include <cassert>

namespace detail::serializer
{

static bool checkSaveLoadCallbacks(
  const std::unordered_map<Component, std::function<void(Entity, std::vector<std::uint8_t>&)>>& save_functions,
  const std::unordered_map<Component, std::function<void(Entity, const std::uint8_t*&)>>&       load_functions) {
    if (save_functions.size() != load_functions.size()) {
        return false;
    }

    auto keys = std::views::keys(save_functions);
    auto it   = std::ranges::find_if(keys, [&load_functions](Entity key) { return !load_functions.contains(key); });

    return it == keys.end();
}

} // namespace detail::serializer

Serializer::Serializer(World& world) : m_world(world) {};

std::vector<std::uint8_t> Serializer::save() {
    assert(detail::serializer::checkSaveLoadCallbacks(m_save_functions, m_load_functions) &&
           "For each save function, you should have one load function");

    spdlog::stopwatch         sw;
    std::vector<std::uint8_t> data;

    for (auto entity : m_world.entities()) {
        const std::vector<std::uint8_t>& id = serializer::serialize(ct::ID<Component>);
        std::ranges::copy(id, std::back_inserter(data));
        for (const auto& func : std::views::values(m_save_functions)) {
            func(entity, data);
        }
    }

    spdlog::info("Saved {:.3}", sw);
    return data;
}


void Serializer::load(std::span<const std::uint8_t> data) {
    assert(detail::serializer::checkSaveLoadCallbacks(m_save_functions, m_load_functions) &&
           "For each save function, you should have one load function");

    if (data.empty()) {
        return;
    }

    spdlog::stopwatch sw;

    const auto* ptr = data.data();

    Entity entity = 0;
    do {
        auto comp_id = *std::launder(reinterpret_cast<const Component*>(ptr));
        ptr += sizeof(IDType);

        if (ct::ID<Entity> == comp_id) {
            entity = m_world.create();
            continue;
        }

        auto it = m_load_functions.find(comp_id);
        if (it != m_load_functions.end()) {
            it->second(entity, ptr);
        }
    } while (ptr - data.data() < data.size()); // NOLINT

    spdlog::info("Loaded {:.3}", sw);
}
