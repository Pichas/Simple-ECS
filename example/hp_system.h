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
    ECS_JOB longTaskExample();

private:
    std::atomic_bool m_sync;
};
