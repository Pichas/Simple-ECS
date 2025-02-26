#pragma once

#include "simple-ecs/world.h"
#include <algorithm>
#include <cstring>
#include <type_traits>


namespace serializer
{

using Data   = char;
using Output = std::vector<Data>;
using Input  = const Data*;


template<typename Type>
requires std::is_trivially_copyable_v<Type>
[[nodiscard]] Output serialize(const Type& obj) {
    constexpr size_t size = sizeof(Type);
    Output           data(size);
    std::memcpy(data.data(), &obj, size);
    return data;
}

template<typename Type>
requires std::is_trivially_copyable_v<Type>
[[nodiscard]] Type deserialize(Input& data) {
    constexpr std::size_t                size = sizeof(Type);
    alignas(Type) std::array<char, size> buffer;

    std::memcpy(&buffer, data, size);
    data += size;

    return *reinterpret_cast<Type*>(std::launder(buffer.data()));
}

}; // namespace serializer

struct Serializer final {
    Serializer(World& world);

    serializer::Output save();
    void               load(std::span<const serializer::Data>);

    template<typename Component>
    requires std::is_trivially_copyable_v<Component>
    void registerType();

    template<typename Component, typename Callback>
    requires std::is_invocable_r_v<serializer::Output, Callback, const Component&>
    void registerCustomSaver(Callback&& f);

    template<typename Component, typename Callback>
    requires std::is_invocable_r_v<Component, Callback, serializer::Input&>
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
    World&                                                                          m_world;
    std::unordered_map<Component, std::function<void(Entity, serializer::Output&)>> m_save_functions;
    std::unordered_map<Component, std::function<void(Entity, serializer::Input&)>>  m_load_functions;
};

template<typename Component>
requires std::is_trivially_copyable_v<Component>
void Serializer::registerType() {
    addSaveCallback<Component>();
    addLoadCallback<Component>();
}

template<typename Component, typename Callback>
requires std::is_invocable_r_v<serializer::Output, Callback, const Component&>
void Serializer::registerCustomSaver(Callback&& f) { // NOLINT
    assert(!m_save_functions.contains(ct::ID<Component>) && "Component already has save function");
    auto [_, was_added] = m_save_functions.try_emplace(
      ct::ID<Component>, [&world = m_world, func = std::forward<Callback>(f)](Entity e, serializer::Output& data) {
          if (world.has<Component>(e)) {
              const Component& comp  = world.get<Component>(e);
              auto&&           bytes = func(comp);

              auto&& id = serializer::serialize(ct::ID<Component>);
              std::ranges::copy(id, std::back_inserter(data));
              std::ranges::copy(bytes, std::back_inserter(data));
          }
      });
    assert(was_added);
}

template<typename Component, typename Callback>
requires std::is_invocable_r_v<Component, Callback, serializer::Input&>
void Serializer::registerCustomLoader(Callback&& f) { // NOLINT
    assert(!m_load_functions.contains(ct::ID<Component>) && "Component already has load function");
    auto [_, was_added] = m_load_functions.try_emplace(
      ct::ID<Component>, [&world = m_world, func = std::forward<Callback>(f)](Entity e, serializer::Input& data) {
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
      m_save_functions.try_emplace(ct::ID<Component>, [&world = m_world](Entity e, serializer::Output& data) {
          if (world.has<Component>(e)) {
              const auto& id = serializer::serialize(ct::ID<Component>);
              std::ranges::copy(id, std::back_inserter(data));
          }
      });
    assert(was_added);
}

template<typename Component>
requires(std::is_empty_v<Component>)
void Serializer::addLoadCallback() {
    auto [_, was_added] =
      m_load_functions.try_emplace(ct::ID<Component>, [&world = m_world](Entity e, serializer::Input& /*fs*/) {
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
