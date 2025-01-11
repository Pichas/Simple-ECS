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

    static inline size_t thread_count = std::thread::hardware_concurrency() / 2;

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

        // wait for all threads to start
        for (auto& is_running : m_threads_control) {
            while (!is_running.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            is_running.store(false, std::memory_order_release);
        }

        std::unique_lock _(m_mutex);
    }

    ECS_FORCEINLINE void triger() {
        ECS_PROFILER(ZoneScoped);

        m_sync.store(!m_sync.load(std::memory_order_relaxed));
        m_sync.notify_all();
    }

    template<typename Filter>
    void registerObserver(std::uint32_t fname) {
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
        for (auto observer_id : m_funcs_to_observers[fname]) {
            --m_observers_in_use[observer_id];
            if (!m_observers_in_use[observer_id]) {
                m_functions[observer_id] = [](World&) {};
            }
        }
    };


private:
    ObserverManager(World& world) : m_threads_control(thread_count) {
        m_threads.reserve(thread_count);

        for (size_t i = 0; i < thread_count; ++i) {
            std::atomic_bool is_started = false;
            m_threads.emplace_back([this, &world, index = i, &is_started](const std::stop_token& stoken) {
                ECS_PROFILER(tracy::SetThreadName("ECS Filter Thread"));

                is_started = true;
                is_started.notify_one();

                while (true) {
                    m_sync.wait(m_sync);
                    std::shared_lock _(m_mutex);
                    m_threads_control[index].store(true, std::memory_order_relaxed);

                    if (stoken.stop_requested()) {
                        return;
                    }

                    ECS_PROFILER(ZoneScoped);

                    for (size_t i = index; i < m_functions.size(); i += thread_count) {
                        std::invoke(m_functions[i], world);
                    }
                }
            });

            // when the game is small, it can start processing systems before init threads
            // so we have to wait all threads to be started
            is_started.wait(false);
        };
    };

private:
    std::vector<std::jthread>                       m_threads;
    std::vector<std::function<void(World&)>>        m_functions;
    std::atomic_bool                                m_sync;
    std::unordered_map<size_t, std::vector<size_t>> m_funcs_to_observers;
    std::unordered_map<size_t, size_t>              m_observers_in_use;
    std::vector<std::atomic_bool>                   m_threads_control;


    ECS_PROFILER(TracySharedLockable(std::shared_mutex, m_mutex));
    ECS_NO_PROFILER(std::shared_mutex m_mutex);
};
