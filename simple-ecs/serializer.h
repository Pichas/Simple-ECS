#pragma once

#include "simple-ecs/world.h"
#include <algorithm>
#include <cstring>
#include <type_traits>


namespace serializer
{

template<typename Type>
requires std::is_trivially_copyable_v<Type>
[[nodiscard]] constexpr std::vector<std::uint8_t> serialize(const Type& obj) {
    auto raw = std::span(reinterpret_cast<const std::uint8_t* const>(&obj), sizeof(Type));
    return {raw.begin(), raw.end()};
}

template<typename Type>
requires std::is_trivially_copyable_v<Type>
[[nodiscard]] constexpr Type deserialize(const std::uint8_t*& data) {
    constexpr std::size_t                        size = sizeof(Type);
    alignas(Type) std::array<std::uint8_t, size> buffer;

    std::memcpy(&buffer, data, size);
    data += size;

    return *reinterpret_cast<Type*>(std::launder(buffer.data()));
}

}; // namespace serializer

struct Serializer final {
    Serializer(World& world);

    std::vector<std::uint8_t> save();
    void                      load(std::span<const std::uint8_t> data);

    template<typename Component>
    requires std::is_trivially_copyable_v<Component>
    void registerType();

    template<typename Component, typename Callback>
    requires std::is_invocable_r_v<std::vector<std::uint8_t>, Callback, const Component&>
    void registerCustomSaver(Callback&& f);

    template<typename Component, typename Callback>
    requires std::is_invocable_r_v<Component, Callback, const std::uint8_t*&>
    void registerCustomLoader(Callback&& f);

private:
    // tags
    template<typename Component>
    requires(std::is_empty_v<Component>)
    void addSaveCallback();

    template<typename Component>
    requires(std::is_empty_v<Component>)
    void addLoadCallback();


    // structs
    template<typename Component>
    requires(!std::is_empty_v<Component>)
    void addSaveCallback();

    template<typename Component>
    requires(!std::is_empty_v<Component>)
    void addLoadCallback();

private:
    World&                                                                                 m_world;
    std::unordered_map<Component, std::function<void(Entity, std::vector<std::uint8_t>&)>> m_save_functions;
    std::unordered_map<Component, std::function<void(Entity, const std::uint8_t*& data)>>  m_load_functions;
};

template<typename Component>
requires std::is_trivially_copyable_v<Component>
void Serializer::registerType() {
    addSaveCallback<Component>();
    addLoadCallback<Component>();
}

template<typename Component, typename Callback>
requires std::is_invocable_r_v<std::vector<std::uint8_t>, Callback, const Component&>
void Serializer::registerCustomSaver(Callback&& f) { // NOLINT
    assert(!m_save_functions.contains(ct::ID<Component>) && "Component already has save function");
    auto [_, was_added] = m_save_functions.try_emplace(
      ct::ID<Component>,
      [&world = m_world, func = std::forward<Callback>(f)](Entity e, std::vector<std::uint8_t>& data) {
          if (world.has<Component>(e)) {
              const Component&                 comp  = world.get<Component>(e);
              const std::vector<std::uint8_t>& bytes = func(comp);

              const std::vector<std::uint8_t>& id = serializer::serialize(ct::ID<Component>);
              std::ranges::copy(id, std::back_inserter(data));
              std::ranges::copy(bytes, std::back_inserter(data));
          }
      });
    assert(was_added);
}

template<typename Component, typename Callback>
requires std::is_invocable_r_v<Component, Callback, const std::uint8_t*&>
void Serializer::registerCustomLoader(Callback&& f) { // NOLINT
    assert(!m_load_functions.contains(ct::ID<Component>) && "Component already has load function");
    auto [_, was_added] = m_load_functions.try_emplace(
      ct::ID<Component>, [&world = m_world, func = std::forward<Callback>(f)](Entity e, const std::uint8_t*& data) {
          Component&& comp = func(data);
          world.emplace(e, std::move(comp));
          world.markUpdated<Component>(e);
      });
    assert(was_added);
}

template<typename Component>
requires(std::is_empty_v<Component>)
void Serializer::addSaveCallback() {
    auto [_, was_added] =
      m_save_functions.try_emplace(ct::ID<Component>, [&world = m_world](Entity e, std::vector<std::uint8_t>& data) {
          if (world.has<Component>(e)) {
              const std::vector<std::uint8_t>& id = serializer::serialize(ct::ID<Component>);
              std::ranges::copy(id, std::back_inserter(data));
          }
      });
    assert(was_added);
}

template<typename Component>
requires(std::is_empty_v<Component>)
void Serializer::addLoadCallback() {
    auto [_, was_added] =
      m_load_functions.try_emplace(ct::ID<Component>, [&world = m_world](Entity e, const std::uint8_t*& /*fs*/) {
          world.emplace<Component>(e);
          world.markUpdated<Component>(e);
      });
    assert(was_added);
}

template<typename Component>
requires(!std::is_empty_v<Component>)
void Serializer::addSaveCallback() {
    registerCustomSaver<Component>(serializer::serialize<Component>);
}

template<typename Component>
requires(!std::is_empty_v<Component>)
void Serializer::addLoadCallback() {
    registerCustomLoader<Component>(serializer::deserialize<Component>);
}
