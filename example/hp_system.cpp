#include "hp_system.h"
#include <simple-ecs/ECS.h>

void HPSystem::setup(Registry& reg) {
    ECS_REG_FUNC(reg, HPSystem::checkHP);
    ECS_REG_FUNC(reg, HPSystem::removeDeadEntity);

    using namespace std::chrono_literals;
    ECS_JOB_RUN(reg, longTaskExample, 100ms);
}

void HPSystem::stop(Registry& reg) {
    ECS_UNREG_FUNC(reg, HPSystem::checkHP);
    ECS_UNREG_FUNC(reg, HPSystem::removeDeadEntity);
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

ECS_JOB HPSystem::longTaskExample() {
    using namespace std::chrono_literals;
    spdlog::warn("Hello from long task");
    spdlog::default_logger()->flush();
    m_sync.store(true);
    m_sync.notify_all();
    return ECS_JOB_CONTINUE;
}
