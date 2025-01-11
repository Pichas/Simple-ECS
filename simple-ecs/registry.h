#pragma once

#include "simple-ecs/base_system.h"
#include "simple-ecs/observer_manager.h"
#include "simple-ecs/serializer.h"
#include "simple-ecs/utils.h"
#include "simple-ecs/world.h"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/stopwatch.h>
#include <algorithm>
#include <chrono>
#include <functional>
#include <queue>
#include <ranges>
#include <tasks/tasks.h>
#include <thread>
#include <utility>


#ifdef ECS_FINAL
#define ECS_FUNCTION_ID(F) #F##_crc
#else
#define ECS_FUNCTION_ID(F) #F
#endif

// clang-format off
#define ECS_REG_FUNC(REGISTRY, FUNC) REGISTRY.registerFunction(ECS_FUNCTION_ID(FUNC), &FUNC, this)
#define ECS_REG_FUNC_SYS(REGISTRY, FUNC, SYSTEM) REGISTRY.registerFunction(ECS_FUNCTION_ID(FUNC), &FUNC, SYSTEM)
#define ECS_REG_EXTERN_FUNC(REGISTRY, FUNC) REGISTRY.registerFunction(ECS_FUNCTION_ID(FUNC), &FUNC)
#define ECS_UNREG_FUNC(REGISTRY, FUNC) REGISTRY.unregisterFunction(ECS_FUNCTION_ID(FUNC))
// clang-format on

#define ECS_JOB_RUN(REGISTRY, FUNC, cycle) \
    REGISTRY.runParallelJob(&std::remove_cvref_t<decltype(*this)>::FUNC, this, cycle)
#define ECS_JOB_CONTINUE true
#define ECS_JOB_STOP false
#define ECS_JOB bool


struct Registry final : NoCopyNoMove {
    Registry(World& world) : m_world(world), m_frame_ready(false), m_serializer(m_world), m_observer_manager(world) {}
    ~Registry() {
        ECS_PROFILER(ZoneScoped);

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
        (m_observer_manager.registerObserver<Filters>(id), ...);
        m_functions.emplace_back(id, f, obj, m_world);
    }

    template<typename... Filters>
    requires(sizeof...(Filters) > 0)
    void registerFunction(std::uint32_t id, void (*f)(OBSERVER(Filters)...)) {
        (m_observer_manager.registerObserver<Filters>(id), ...);
        m_functions.emplace_back(id, f, m_world);
    }

    void unregisterFunction(std::uint32_t id) {
        m_observer_manager.unregisterObserver(id);
        m_cleanup_callbacks.emplace([this, id] { std::erase(m_functions, id); });
    }
#else
    template<typename System, typename... Filters>
    requires(sizeof...(Filters) > 0 && std::derived_from<System, BaseSystem>)
    void registerFunction(std::string_view fname, void (System::*f)(OBSERVER(Filters)...), System* obj) {
        ECS_PROFILER(ZoneScoped);

        bool exists = std::ranges::find(m_functions, fname) != m_functions.end();
        if (exists) {
            spdlog::critical("{} function is already registered", fname);
            assert(false);
        } else {
            spdlog::debug("{} function was registered", fname);
        }

        (m_observer_manager.registerObserver<Filters>(crc32::compute(fname)), ...);
        m_functions.emplace_back(fname, f, obj, m_world);
    }

    template<typename... Filters>
    requires(sizeof...(Filters) > 0)
    void registerFunction(std::string_view fname, void (*f)(OBSERVER(Filters)...)) {
        ECS_PROFILER(ZoneScoped);

        bool exists = std::ranges::find(m_functions, fname) != m_functions.end();
        if (exists) {
            spdlog::critical("{} function is already registered", fname);
            assert(false);
        } else {
            spdlog::debug("{} function was registered", fname);
        }

        (m_observer_manager.registerObserver<Filters>(crc32::compute(fname)), ...);
        m_functions.emplace_back(fname, f, m_world);
    }

    void unregisterFunction(std::string_view fname) {
        ECS_PROFILER(ZoneScoped);

        bool exists = std::ranges::find(m_functions, fname) != m_functions.end();
        if (!exists) {
            spdlog::critical("{} function is already unregistered", fname);
            assert(false);
        } else {
            spdlog::debug("{} function was unregistered", fname);
        }
        m_observer_manager.unregisterObserver(crc32::compute(fname));
        m_cleanup_callbacks.emplace([this, fname] { std::erase(m_functions, fname); });
    }
#endif

    template<typename System, typename Time>
    requires std::derived_from<System, BaseSystem>
    void runParallelJob(ECS_JOB (System::*func)(), System* obj, Time every) {
        ECS_PROFILER(ZoneScoped);

        using namespace std::literals;

        assert(every >= 100ms && "Doesn't support time less than 100ms");
        m_parallel_jobs[ct::ID<Component>].emplace_back(
          [](const std::stop_token& stoken, ECS_JOB (System::*function)(), System* obj, Time time) {
              ECS_PROFILER(ZoneScoped);

              spdlog::stopwatch sw;

              while (!stoken.stop_requested()) {
                  ECS_PROFILER(ZoneScoped);

                  if (sw.elapsed() > time) {
                      sw.reset();
                      if (std::invoke(function, obj) == ECS_JOB_STOP) {
                          return;
                      }
                  }
                  std::this_thread::sleep_for(time - sw.elapsed());
              }
          },
          func,
          obj,
          every);

        spdlog::debug("Job for {} was started", ct::name<System>);
    }

    template<typename System, typename... Args>
    requires std::derived_from<System, BaseSystem>
    System* addSystem(Args&&... args) {
        ECS_PROFILER(ZoneScoped);

        assert(!m_systems.contains(ct::ID<System>) && "system is already registered");
        spdlog::debug("register: {}", ct::name_sv<System>);

        auto system     = std::make_unique<System>(std::forward<Args>(args)...);
        auto system_ptr = system.get();
        m_init_callbacks.emplace([system_ptr, this]() {
            spdlog::debug("init: {}", ct::name_sv<System>);
            system_ptr->setup(*this);
        });

        auto [it, was_added] = m_systems.try_emplace(ct::ID<System>, std::move(system));
        assert(was_added);
        return system_ptr;
    }

    template<typename System>
    requires std::derived_from<System, BaseSystem>
    void removeSystem() {
        ECS_PROFILER(ZoneScoped);

        auto system = m_systems.find(ct::ID<System>);
        assert(system != m_systems.end() && "system is already unregistered");

        // stop system
        m_cleanup_callbacks.emplace([system = std::move(system), this] {
            spdlog::debug("remove: {}", ct::name_sv<System>);
            system->second.get()->stop(*this);
            m_parallel_jobs.erase(ct::ID<System>);
            m_systems.erase(system);
        });
    }

    template<typename System>
    requires std::derived_from<System, BaseSystem>
    std::remove_cvref_t<System>* getSystem() noexcept {
        ECS_PROFILER(ZoneScoped);

        if (auto system = m_systems.find(ct::ID<System>); system != m_systems.end()) {
            return static_cast<System*>(system->second.get());
        }
        return nullptr;
    }

    void initNewSystems() {
        ECS_PROFILER(ZoneScoped);

        while (!m_init_callbacks.empty()) {
            // can be called recursively, so move and call
            auto init = std::move(m_init_callbacks.front());
            m_init_callbacks.pop();
            init();
        }
    }

    void syncWithRender() noexcept {
        ECS_PROFILER(ZoneScoped);

        while (m_frame_ready.load(std::memory_order_relaxed)) {}
    }

    void frameSynchronized() noexcept {
        ECS_PROFILER(ZoneScoped);

        m_frame_ready.store(false, std::memory_order_relaxed);
    }

    void waitFrame() noexcept {
        ECS_PROFILER(ZoneScoped);

        while (!m_frame_ready.load(std::memory_order_relaxed)) {}
    }

    void prepare() noexcept { m_observer_manager.triger(); }

    void exec() noexcept {
        ECS_PROFILER(ZoneScoped);

        assert(m_init_callbacks.empty() && "all systems must be initialized");

        m_observer_manager.sync();
        for (const auto& function : m_functions) {
            function();
        }

        cleanup();
        m_world.flush();    // destroy all removed entities at the end of the frame
        m_world.optimise(); // make storage optimisations

        m_frame_ready.store(true, std::memory_order_relaxed);
    }

    // save including filtering time
    std::vector<std::pair<double, std::string_view>> getRegisteredFunctionsInfo() {
#ifdef ECS_FINAL
        return {};
#else
        std::vector<std::pair<double, std::string_view>> names;
        names.reserve(m_functions.size());
        for (const auto& func : m_functions) {
            names.emplace_back(func.executionTime(), func.name());
        }
        return names;
#endif
    }


private:
    struct Function final {
        Function(const Function& other)                = delete;
        Function(Function&& other) noexcept            = default;
        Function& operator=(const Function& other)     = delete;
        Function& operator=(Function&& other) noexcept = default;
        ~Function() noexcept                           = default;

        template<typename System, typename... Filters>
        Function(ECS_FINAL_SWITCH(std::uint32_t, std::string_view) id, //
                 void (System::*f)(OBSERVER(Filters)...),
                 System* obj,
                 World&  world)
          : m_function([f, obj, &world] { std::invoke(f, obj, ObserverManager::observers<Filters>(world)...); })
          , m_id(id){};

        template<typename... Filters>
        Function(ECS_FINAL_SWITCH(std::uint32_t, std::string_view) id, //
                 void (*f)(OBSERVER(Filters)...),
                 World& world)
          : m_function([f, &world] { std::invoke(f, ObserverManager::observers<Filters>(world)...); }), m_id(id) {}

        void operator()() const {
            ECS_PROFILER(ZoneScoped);

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
        ECS_PROFILER(ZoneScoped);

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
    ObserverManager                                         m_observer_manager;
};