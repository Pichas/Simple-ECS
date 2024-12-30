#pragma once

#include "components.h"
#include <simple-ecs/ECS.h>


struct HPSystem : BaseSystem {
    void setup(Registry&);
    void stop(Registry& /*unused*/) override;

private:
    using CheckHPFilter          = Filter<Require<HP>, Exclude<Dead>>;
    using RemoveDeadEntityFilter = Filter<Require<Dead>>;

    void checkHP(OBSERVER(CheckHPFilter));
    void removeDeadEntity(OBSERVER(RemoveDeadEntityFilter));

private:
    ECS_JOB longTaskExample() {
        using namespace std::chrono_literals;
        spdlog::warn("Hello from long task");
        spdlog::default_logger()->flush();
        m_sync.test_and_set();
        m_sync.notify_all();
        return ECS_JOB_CONTINUE;
    }

    std::atomic_flag m_sync;
};
