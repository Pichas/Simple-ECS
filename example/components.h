#pragma once

#include "simple-ecs/components.h"
#include "simple-ecs/entity_debug.h"

struct Dead {};

struct Player {};

struct Boss {};

struct Damage {
    int damage = 0;
};

struct HP {
    int hp = 0;
};


using PlayerArchetype = Archetype<Name, HP, Damage, Player>;
struct PlayerType : PlayerArchetype {
    PlayerType() : PlayerArchetype({"Player"}, {100}, {3}){};
};

using BossArchetype = Archetype<Name, HP, Damage, Boss>;
struct BossType : BossArchetype {
    BossType() : BossArchetype({"Boss"}, {1000}, {10}){};
};
