#pragma once

#include "components.h"
#include <simple-ecs/ECS.h>


struct HPSystem : BaseSystem {
    void setup(Registry&);

private:
    using ApplyDamageFilter      = Filter<Require<HP, Damage>, Exclude<Dead>>;
    using RemoveDeadEntityFilter = Filter<Require<Dead>>;
    using GetNewFilter           = Filter<Require<PlayerArchetype>>;

    void applyDamage(OBSERVER(ApplyDamageFilter));
    void removeDeadEntity(OBSERVER(RemoveDeadEntityFilter));
    void getNewEntity(OBSERVER(GetNewFilter));
};
