#pragma once

#include "simple-ecs/base_system.h"
#include "simple-ecs/observer.h"
#include "simple-ecs/serializer.h"
#include "simple-ecs/utils.h"
#include "simple-ecs/world.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/stopwatch.h>
#include <algorithm>
#include <chrono>
#include <functional>
#include <queue>
#include <thread>
#include <utility>


#ifndef ECS_FINAL
#define ECS_FUNCTION_ID(F) #F
#else
#define ECS_FUNCTION_ID(F) #F##_crc
#endif

#define ECS_REG_FUNC(REGISTRY, FUNC) REGISTRY.registerFunction(ECS_FUNCTION_ID(FUNC), &FUNC, this)
#define ECS_REG_EXTERN_FUNC(REGISTRY, FUNC) REGISTRY.registerFunction(ECS_FUNCTION_ID(FUNC), &FUNC)
#define ECS_UNREG_FUNC(REGISTRY, FUNC) REGISTRY.unregisterFunction(ECS_FUNCTION_ID(FUNC))

#define ECS_JOB_RUN(REGISTRY, FUNC, cycle) REGISTRY.runParallelJob(&FUNC, this, cycle)
#define ECS_JOB_CONTINUE true
#define ECS_JOB_STOP false
#define ECS_JOB bool


struct Registry final : NoCopyNoMove {
    Registry(World& world) : m_world(world), m_frame_ready(false), m_serializer(m_world) {}
    ~Registry() {
        for (const auto& system : std::views::values(m_systems)) {
            system->stop(*this);
        }
        m_parallel_jobs.clear();
        cleanup();
    }

    Serializer& serializer() noexcept { return m_serializer; }


#ifdef ECS_FINAL
    template<typename System, typename... Filters>
    requires(sizeof...(Filters) > 0 && std::derived_from<System, BaseSystem>)
    void registerFunction(std::uint32_t id, void (System::*f)(OBSERVER(Filters)...), System* obj) {
        m_functions.emplace_back(id, f, obj, m_world);
    }

    template<typename... Filters>
    requires(sizeof...(Filters) > 0)
    void registerFunction(std::uint32_t id, void (*f)(OBSERVER(Filters)...)) {
        m_functions.emplace_back(id, f, m_world);
    }

    void unregisterFunction(std::uint32_t id) {
        m_cleanup_callbacks.emplace([this, id] { std::erase(m_functions, id); });
    }
#else
    template<typename System, typename... Filters>
    requires(sizeof...(Filters) > 0 && std::derived_from<System, BaseSystem>)
    void registerFunction(std::string_view fname, void (System::*f)(OBSERVER(Filters)...), System* obj) {
        bool exists = std::ranges::find(m_functions, fname) != m_functions.end();
        if (exists) {
            spdlog::critical("{} function is already registered", fname);
            assert(false);
        } else {
            spdlog::debug("{} function was registered", fname);
        }
        m_functions.emplace_back(fname, f, obj, m_world);
    }

    template<typename... Filters>
    requires(sizeof...(Filters) > 0)
    void registerFunction(std::string_view fname, void (*f)(OBSERVER(Filters)...)) {
        bool exists = std::ranges::find(m_functions, fname) != m_functions.end();
        if (exists) {
            spdlog::critical("{} function is already registered", fname);
            assert(false);
        } else {
            spdlog::debug("{} function was registered", fname);
        }
        m_functions.emplace_back(fname, f, m_world);
    }

    void unregisterFunction(std::string_view fname) {
        bool exists = std::ranges::find(m_functions, fname) != m_functions.end();
        if (!exists) {
            spdlog::critical("{} function is already unregistered", fname);
            assert(false);
        } else {
            spdlog::debug("{} function was unregistered", fname);
        }
        m_cleanup_callbacks.emplace([this, fname] { std::erase(m_functions, fname); });
    }
#endif

    template<typename System, typename Time>
    requires std::derived_from<System, BaseSystem>
    void runParallelJob(bool (System::*func)(), System* obj, Time every) {
        using namespace std::literals;

        assert(every >= 100ms && "Doesn't support time less than 100ms");
        m_parallel_jobs[ID<Component>].emplace_back(
          [](const std::stop_token& stoken, bool (System::*function)(), System* obj, Time time) {
              spdlog::stopwatch sw;

              while (!stoken.stop_requested()) {
                  if (sw.elapsed() > time) {
                      sw.reset();
                      if (!std::invoke(function, obj)) {
                          return;
                      }
                  }

                  std::this_thread::sleep_for(100ms);
              }
          },
          func,
          obj,
          every);

        spdlog::debug("Job for {} was started", S_NAME<System>);
    }

    template<typename System, typename... Args>
    requires std::derived_from<System, BaseSystem>
    void addSystem(Args&&... args) {
        assert(!m_systems.contains(ID<System>) && "system is already registered");
        spdlog::debug("register: {}", SV_NAME<System>);

        auto system = std::make_unique<System>(std::forward<Args>(args)...);
        m_init_callbacks.emplace([system = system.get(), this]() {
            spdlog::debug("init: {}", SV_NAME<System>);
            system->setup(*this);
        });

        m_systems.emplace(ID<System>, std::move(system));
    }

    template<typename System>
    requires std::derived_from<System, BaseSystem>
    void removeSystem() {
        auto system = m_systems.find(id<System>());
        assert(system != m_systems.end() && "system is already unregistered");

        // stop system
        m_cleanup_callbacks.emplace([system_ptr = system->second.get(), this] {
            spdlog::debug("remove: {}", SV_NAME<System>);
            system_ptr->stop(*this);
            m_parallel_jobs.erase(system_ptr);
            m_systems.erase(system);
        });
    }

    template<typename System>
    requires std::derived_from<System, BaseSystem>
    std::remove_cvref_t<System>* getSystem() noexcept {
        if (auto system = m_systems.find(ID<System>); system != m_systems.end()) {
            return static_cast<System*>(system->second.get());
        }
        return nullptr;
    }

    void initNewSystems() {
        while (!m_init_callbacks.empty()) {
            // can be called recursively, so move and call
            auto init = std::move(m_init_callbacks.front());
            m_init_callbacks.pop();
            init();
        }
    }

    void syncWithRender() noexcept {
        while (m_frame_ready.load(std::memory_order_relaxed)) {}
    }

    void frameSynchronized() noexcept { m_frame_ready.store(false, std::memory_order_relaxed); }

    void waitFrame() noexcept {
        while (!m_frame_ready.load(std::memory_order_relaxed)) {}
    }

    void exec() noexcept {
        assert(m_init_callbacks.empty() && "all systems must be initialized");

        for (const auto& function : m_functions) {
            function();
        }

        cleanup();
        m_world.flush(); // destroy all removed entities at the end of the frame

        m_frame_ready.store(true, std::memory_order_relaxed);
    }

    // save including filtering time
    std::vector<std::pair<double, std::string_view>> getRegisteredFunctionsInfo() {
#ifndef ECS_FINAL
        std::vector<std::pair<double, std::string_view>> names;
        names.reserve(m_functions.size());
        for (const auto& func : m_functions) {
            names.emplace_back(func.executionTime(), func.name());
        }
        return names;
#else
        return {};
#endif
    }


private:
    struct Function final {
        Function(Function&& other) noexcept                 = default;
        Function(const Function& other) noexcept            = delete;
        Function& operator=(Function&& other) noexcept      = default;
        Function& operator=(const Function& other) noexcept = delete;
        ~Function() noexcept                                = default;

        template<typename System, typename... Filters>
        Function(ECS_FINAL_SWITCH(std::uint32_t, std::string_view) id, //
                 void (System::*f)(OBSERVER(Filters)...),
                 System* obj,
                 World&  world)
          : m_function([f, obj, &world] { std::invoke(f, obj, Observer<Filters>(world)...); }), m_id(id){};

        template<typename... Filters>
        Function(ECS_FINAL_SWITCH(std::uint32_t, std::string_view) id, //
                 void (*f)(OBSERVER(Filters)...),
                 World& world)
          : m_function([f, &world] { std::invoke(f, Observer<Filters>(world)...); }), m_id(id) {}

        void operator()() const {
            ECS_NOT_FINAL_ONLY(spdlog::stopwatch sw);
            m_function();
            ECS_NOT_FINAL_ONLY(m_time = sw.elapsed());
        }

        ECS_FINAL_ONLY(operator std::uint32_t() const { return m_id; })

        ECS_NOT_FINAL_ONLY(bool operator==(const Function& rhs) const noexcept { return m_id == rhs.m_id; })
        ECS_NOT_FINAL_ONLY(operator std::string_view() const noexcept { return m_id; })
        ECS_NOT_FINAL_ONLY(operator std::string() const { return {m_id.data(), m_id.size()}; })
        ECS_NOT_FINAL_ONLY(std::string_view name() const noexcept { return m_id; })
        ECS_NOT_FINAL_ONLY(double executionTime() const noexcept { return m_time.count(); })

    private:
        std::function<void(void)> m_function;
        ECS_FINAL_SWITCH(std::uint32_t, std::string_view) m_id;
        ECS_NOT_FINAL_ONLY(mutable std::chrono::duration<double> m_time{});
    };

    void cleanup() noexcept {
        while (!m_cleanup_callbacks.empty()) {
            auto func = std::move(m_cleanup_callbacks.front());
            m_cleanup_callbacks.pop();
            func();
        }
    }

private:
    World&                                                  m_world;
    std::vector<Function>                                   m_functions;
    std::queue<std::function<void(void)>>                   m_init_callbacks;
    std::queue<std::function<void(void)>>                   m_cleanup_callbacks;
    std::unordered_map<SystemID, std::unique_ptr<System>>   m_systems;
    std::unordered_map<SystemID, std::vector<std::jthread>> m_parallel_jobs;
    std::atomic_bool                                        m_frame_ready;
    Serializer                                              m_serializer;
};