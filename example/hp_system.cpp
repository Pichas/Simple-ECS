#include "hp_system.h"
#include <simple-ecs/ECS.h>

void HPSystem::setup(Registry& reg) {
    ECS_REG_FUNC(reg, applyDamage);
    ECS_REG_FUNC(reg, removeDeadEntity);
    ECS_REG_FUNC(reg, getNewEntity);
}

void HPSystem::applyDamage(OBSERVER(ApplyDamageFilter) observer) {
    for (auto e : observer) {
        auto [hp, dmg] = e.asTuple();
        hp.hp -= dmg.damage;

        spdlog::info("{}", hp.hp);

        if (hp.hp <= 0) {
            e.emplace<Dead>();
            e.emplace<PlayerName>({"TEST"});
            auto n = observer.create();
            n.emplace<PlayerName>();
            n.emplace<HP>();
            auto n2 = observer.create();
            n2.emplace<PlayerName>("Test");
            n2.emplace<HP>();

            n.destroy();
            n2.destroy();

            observer.create<PlayerArchetype>();
            observer.create<PlayerType>();
        }
    }
    observer.erase<Damage>();
};

void HPSystem::removeDeadEntity(OBSERVER(RemoveDeadEntityFilter) observer) {
    for (auto e : observer) {
        if (e.isAlive()) {
            spdlog::info("{} is dead", static_cast<Entity>(e));
            e.destroy();
        }
    }
}

void HPSystem::getNewEntity(OBSERVER(GetNewFilter) observer) {
    for (auto e : observer) {
        if (e.isAlive()) {
            const auto& name = e.get<PlayerName>().name;
            spdlog::info("player {} - {} is dead", name, static_cast<Entity>(e));
            e.destroy();
        }
    }
}
