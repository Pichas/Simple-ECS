#include "hp_system.h"
#include <simple-ecs/ECS.h>

void HPSystem::setup(Registry& reg) {
    ECS_REG_FUNC(reg, checkHP);
    ECS_REG_FUNC(reg, removeDeadEntity);

    using namespace std::chrono_literals;
    ECS_JOB_RUN(reg, longTaskExample, 100ms);
}

void HPSystem::stop(Registry& reg) {
    ECS_UNREG_FUNC(reg, checkHP);
    ECS_UNREG_FUNC(reg, removeDeadEntity);
    m_sync.wait(false);
}

void HPSystem::checkHP(OBSERVER(CheckHPFilter) observer) {
    for (auto e : observer) {
        const auto& hp = e.get<HP>();

        if (hp.hp <= 0) {
            e.emplace<Dead>();
        }
    }
};

void HPSystem::removeDeadEntity(OBSERVER(RemoveDeadEntityFilter) observer) {
    for (auto e : observer) {
        if (e.isAlive()) {
            auto* name = e.tryGet<Name>();
            spdlog::info("{} is dead", name ? name->name : std::to_string(e));
            e.destroy();
        }
    }
}
