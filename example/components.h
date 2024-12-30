#pragma once

#include "simple-ecs/components.h"
#include <string>

struct Dead {};

struct Damage {
    int damage = 0;
};

struct HP {
    int hp = 0;
};

struct PlayerName {
    std::string name;
};

using PlayerArchetype = Archetype<HP, PlayerName>;

struct PlayerType : PlayerArchetype {
    PlayerType() : PlayerArchetype(HP(100), PlayerName("Test")) {};
};