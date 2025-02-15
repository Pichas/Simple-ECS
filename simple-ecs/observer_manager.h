#pragma once

#include "simple-ecs/observer.h"
#include "simple-ecs/utils.h"
#include <shared_mutex>


namespace detail::observer
{

inline IDType nextID() {
    static IDType id = 0;
    return id++;
}

template<typename T>
inline IDType sequenceID() {
    static IDType id = nextID();
    return id;
};

} // namespace detail::observer


struct ObserverManager : NoCopyNoMove {
    friend struct Registry;

    static inline size_t thread_count = std::thread::hardware_concurrency();

    template<typename Filter>
    ECS_FORCEINLINE static Observer<Filter>& observers(World& world) {
        static Observer<Filter> observer{world};
        return observer;
    };

public:
    ~ObserverManager() noexcept {
        for (auto& thread : m_threads) {
            thread.request_stop();
        }

        triger();
        m_threads.clear();
    }

    ECS_FORCEINLINE void sync() {
        ECS_PROFILER(ZoneScoped);

        // guarantee that all functions were finished
        while (m_finished_function.load(std::memory_order_relaxed) != m_functions.size()) {
            std::this_thread::yield();
        }
    }

    ECS_FORCEINLINE void triger() {
        ECS_PROFILER(ZoneScoped);

        m_current_function.store(0, std::memory_order_relaxed);
        m_finished_function.store(0, std::memory_order_relaxed);
        m_sync.store(!m_sync.load(std::memory_order_acquire), std::memory_order_release);
        m_sync.notify_all();
    }

    template<typename Filter>
    void registerObserver(std::uint32_t fname) {
        std::unique_lock _(m_mutex);

        auto observer_id = detail::observer::sequenceID<Filter>();

        m_funcs_to_observers[fname].emplace_back(observer_id);
        m_observers_in_use[observer_id]++;


        auto function = [](World& world) { observers<Filter>(world).refresh(); };
        if (m_functions.size() == observer_id) {
            m_functions.emplace_back(std::move(function));
        }
        if (m_observers_in_use[observer_id]) {
            m_functions[observer_id] = std::move(function);
        }
    };

    void unregisterObserver(std::uint32_t fname) {
        std::unique_lock _(m_mutex);

        for (auto observer_id : m_funcs_to_observers[fname]) {
            assert(m_observers_in_use[observer_id]);
            --m_observers_in_use[observer_id];
            if (!m_observers_in_use[observer_id]) {
                m_functions[observer_id] = [](World&) {};
            }
        }
    };


private:
    ObserverManager(World& world) : m_sync(false) {
        m_threads.reserve(thread_count);

        for (size_t i = 0; i < thread_count; ++i) {
            auto& thread = m_threads.emplace_back([this, &world](const std::stop_token& stoken) {
                ECS_PROFILER(tracy::SetThreadName("ECS Filter Thread"));

                while (true) {
                    m_sync.wait(m_sync.load(std::memory_order_relaxed), std::memory_order_acquire);
                    std::shared_lock _(m_mutex);

                    if (stoken.stop_requested()) {
                        return;
                    }

                    ECS_PROFILER(ZoneScoped);

                    for (auto i = m_current_function.fetch_add(1, std::memory_order_relaxed); //
                         static_cast<size_t>(i) < m_functions.size();
                         i = m_current_function.fetch_add(1, std::memory_order_relaxed)) {
                        std::invoke(m_functions[i], world);
                        m_finished_function.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });

            // we have to wait all threads to be started before start processing
            while (!thread.joinable()) {
                std::this_thread::yield();
            }
        };
    };

private:
    std::vector<std::jthread>                       m_threads;
    std::unordered_map<size_t, std::vector<size_t>> m_funcs_to_observers;
    std::unordered_map<size_t, size_t>              m_observers_in_use;
    std::vector<std::function<void(World&)>>        m_functions;
    std::atomic_uint16_t                            m_current_function;
    std::atomic_uint16_t                            m_finished_function;
    std::atomic_bool                                m_sync;

    ECS_PROFILER(TracySharedLockable(std::shared_mutex, m_mutex));
    ECS_NO_PROFILER(std::shared_mutex m_mutex);
};
