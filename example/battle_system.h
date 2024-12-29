#pragma once

#include "components.h"
#include <simple-ecs/ECS.h>


struct BattleSystem : BaseSystem {
    void setup(Registry&);
    void stop(Registry& /*unused*/) override;

private:
    using PlayerFilter = Filter<Require<PlayerArchetype>, Exclude<Dead>>;
    using BossFilter   = Filter<Require<BossArchetype>, Exclude<Dead>>;

    void hitBoss(OBSERVER(PlayerFilter), OBSERVER(BossFilter));
    void hitPlayers(OBSERVER(PlayerFilter), OBSERVER(BossFilter));
};
