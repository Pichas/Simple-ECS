#pragma once

#include "simple-ecs/utils.h"

struct Registry;
struct BaseSystem;

using System     = BaseSystem;
using SystemType = BaseSystem;
using SystemID   = IDType;

struct BaseSystem : NoCopyNoMove {
    BaseSystem()                  = default;
    virtual ~BaseSystem()         = default;
    void         setup(Registry&) = delete;
    virtual void stop(Registry& /*unused*/) {};
};
