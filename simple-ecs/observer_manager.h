#pragma once


#include "simple-ecs/observer.h"
#include "simple-ecs/utils.h"


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
    static inline size_t thread_count = std::thread::hardware_concurrency();

    ObserverManager(World& world) {
        m_threads.reserve(thread_count);

        for (size_t i = 0; i < thread_count; ++i) {
            m_threads.emplace_back([this, &world, index = i](const std::stop_token& stoken) {
                while (true) {
                    m_sync.wait(m_sync);

                    ECS_PROFILER(ZoneScoped);

                    if (stoken.stop_requested()) {
                        return;
                    }

                    for (size_t i = index; i < m_functions.size(); i += thread_count) {
                        m_functions[i](world);
                        m_counter.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        };
    };

    ~ObserverManager() noexcept {
        for (auto& thread : m_threads) {
            thread.request_stop();
        }

        triger();
        m_threads.clear();
    }

    ECS_FORCEINLINE void update() {
        ECS_PROFILER(ZoneScoped);

        triger();

        while (m_counter.load(std::memory_order_relaxed) != m_functions.size()) {
            std::this_thread::yield();
        }
    }

    ECS_FORCEINLINE void triger() {
        m_counter.store(0, std::memory_order_relaxed);
        m_sync.store(!m_sync);
        m_sync.notify_all();
    }

    template<typename Filter>
    void registerObserver() {
        m_functions.emplace_back([](World& world) { observers<Filter>(world).refresh(); });
    };


private:
    template<typename Filter>
    ECS_FORCEINLINE static Observer<Filter>& observers(World& world) {
        static Observer<Filter> observer{world};
        return observer;
    };

private:
    std::vector<std::jthread>                m_threads;
    std::vector<std::function<void(World&)>> m_functions;
    std::atomic_uint32_t                     m_counter;
    std::atomic_bool                         m_sync;
};
