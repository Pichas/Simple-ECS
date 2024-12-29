#include "battle_system.h"
#include <simple-ecs/ECS.h>
#include "ct/random.h"

void BattleSystem::setup(Registry& reg) {
    ECS_REG_FUNC(reg, BattleSystem::hitBoss);
    ECS_REG_FUNC(reg, BattleSystem::hitPlayers);
}

void BattleSystem::stop(Registry& reg) {
    ECS_UNREG_FUNC(reg, BattleSystem::hitBoss);
    ECS_UNREG_FUNC(reg, BattleSystem::hitPlayers);
}

void BattleSystem::hitBoss(OBSERVER(PlayerFilter) players, OBSERVER(BossFilter) boss) {
    if (players.empty() || boss.empty()) {
        return;
    }

    const auto& first_boss = *boss.begin();
    auto&       boss_hp    = first_boss.get<HP>();
    auto&       boss_name  = first_boss.get<Name>();

    for (const auto& e : players) {
        const auto& [name, hp, damage] = e.get();

        if (dice(0, 1)) { // 50% hit
            spdlog::info("{} hit {} HP {} (-{})", name.name, boss_name.name, boss_hp.hp, damage.damage);
            boss_hp.hp -= damage.damage;
        } else {
            spdlog::info("{} miss", name.name);
        }
    }
};

void BattleSystem::hitPlayers(OBSERVER(PlayerFilter) players, OBSERVER(BossFilter) boss) {
    if (players.empty() || boss.empty()) {
        return;
    }

    // closest boss only hits
    const auto& first_boss         = *boss.begin();
    const auto& [name, hp, damage] = first_boss.get();

    // one hit to random player
    if (!players.empty()) {
        const auto& random_player = dice<size_t>(0, players.size() - 1);
        const auto& player        = players[random_player];
        auto&       player_hp     = player.get<HP>();
        auto&       player_name   = player.get<Name>();
        if (dice(0, 1)) { // 50% hit
            spdlog::info("{} hit {} HP {} (-{})", name.name, player_name.name, player_hp.hp, damage.damage);
            player_hp.hp -= damage.damage;
        } else {
            spdlog::info("{} miss", name.name);
        }
    }
};
