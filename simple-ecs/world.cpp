#include "simple-ecs/world.h"
#include "simple-ecs/entity_debug.h"
#include "simple-ecs/registry.h"


World::World() {
    m_reg = std::make_unique<Registry>(*this);
    m_reg->addSystem<EntityDebugSystem>(*this);
}
