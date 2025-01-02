# Simple-ECS

One more ECS. You can check [this](https://gitlab.com/p34ch-main/Game) repo for use case.

## How to use

### World

The first step is to create a new `World`.

```cpp
World world;
```

### Registry

The `World` has a `Registry` inside. The `Registry` adds systems and functions for execution. Also it has `exec` method to calculate one frame.

```cpp
auto* reg = world.getRegistry();
reg->addSystem<MySystem>(/*args...*/);
reg->initNewSystems();

// after you have some systems you can get access to them
auto* my_system = reg.getSystem<MySystem>();
assert(my_system);

while(true) reg->exec();

reg.removeSystem<MySystem>();
```

### Systems

To create your own system you need to derive from `BaseSystem` class and define `void setup(Registry&)` function. You also can override `virtual void stop(Registry&)` if you want to execute code before `System` will be removed from registry.

```cpp
struct MySystem final : BaseSystem {
    MySystem()           = default;
    ~MySystem() override = default;
    void setup(Registry&);
    void stop(Registry&) override;
};
```

#### Functions

Each system can have functions to modify components. You need to provide filters to get entities from the `World`.

```cpp
struct MySystem final : BaseSystem {
    //...
private:
    using UpdateTransformFilter = Filter<Require<Transform>, Exclude<Camera>>;
    using UpdateCameraTransformFilter = Filter<Require<Transform, Camera>>;

    void updateTransform(OBSERVER(UpdateTransformFilter));
    void updateCameraTransform(OBSERVER(UpdateCameraTransformFilter));
    // you can have more than one filter
    void update(OBSERVER(UpdateTransformFilter), OBSERVER(UpdateCameraTransformFilter));
}
```

```cpp
void MySystem::updateTransform(OBSERVER(UpdateTransformFilter) observer) {
    for (auto e : observer) {
        auto& [translation, rotation, scale] = e.get<Transform>();
        translation = {0, 1, 2};
    }
}

void MySystem::updateCameraTransform(OBSERVER(UpdateCameraTransformFilter) observer) {
    for (auto e : observer) {
        auto& [translation, rotation, scale] = e.get<Transform>();
        auto& [pitch, yaw, view]             = e.get<Camera>();
        translation = {4, 5, 6};
        pitch = 42.f;
    }
}
```

#### Register function

```cpp
// OBSERVER_EMPTY is for execute function every frame without entities. You can use observer to create a new entity.
void func(OBSERVER_EMPTY) {}


void MySystem::setup(Registry& reg) {
    ECS_REG_FUNC(reg, MySystem::updateTransform);
    ECS_REG_FUNC(reg, MySystem::updateCameraTransform);
    ECS_REG_FUNC(reg, MySystem::update); // same registration for all functions

    // to register external function use ECS_REG_EXTERN_FUNC macro.
    ECS_REG_EXTERN_FUNC(reg, func);
}

void MySystem::stop(Registry& reg) {
    // when you want to remove function from execution you can use
    ECS_UNREG_FUNC(reg, MySystem::updateTransform);
    ECS_UNREG_FUNC(reg, func);
}
```

Or you can register your function from outside.

```cpp
auto& reg = *world.getRegistry();

auto* my_system = reg.addSystem<MySystem>();
ECS_REG_FUNC_SYS(reg, MySystem::updateTransform, my_system);
ECS_REG_FUNC_SYS(reg, MySystem::updateCameraTransform, my_system);
ECS_REG_FUNC_SYS(reg, MySystem::update, my_system);

reg.initNewSystems();
```

### Components

You can use any type as component, but you need to register it before. To do this we have a `ComponentRegistrant` helper class. You can check `entity_debug.cpp` for examples.

```cpp
struct Camera {
    float     pitch;
    float     yaw;
    glm::mat4 view;
};

struct Transform {
    glm::vec3 translation;
    glm::vec3 rotation;
    glm::vec3 scale;
};

//...

ComponentRegistrant<Transform, Camera>(m_world)
      .createStorage(); // without storage you cannot use components
```

### Work with Entities and Components

Check `observer.h` or `world.h` for more information

```cpp
void MySystem::func(OBSERVER(Filter) observer) {
    for (auto e : observer) {
        // you can get access to all components like this
        auto [Transform, Camera] = e.get();
        // NOTE: Tags will be ignored and you will get only components with data
        // WARN: returns std::tuple<T&, ...>


        // you can create a new empty entity
        auto new_entity = observer.create();

        // entity.func<T>() is eq of observer.func<T>(e)

        // assign components
        new_entity.emplace<Transform>({});
        // or
        new_entity.emplace(Camera{});
        // or
        new_entity.forceEmplace<Camera>({}); // to override if exists
        // or
        new_entity.emplaceTagged<Camera>({}); // emplace component and tag Updated<Camera>

        new_entity.markUpdated<Camera>(); // add tag Updated<Camera>

        new_entity.clearUpdateTag<Camera>(); // remove tag Updated<Camera>

        // NOTE: you can use Updated<T> tag to only get components you marked Updated

        // get by ref for modify or const ref to read only (must be in Requires and not in Exclude)
        auto& camera = new_entity.get<Camera>();
        auto* camera_ptr = new_entity.tryGet<Camera>(); // can be used without restrictions

        // check if Entity has Component
        bool has_camera = new_entity.has<Camera>();

        // erase components
        new_entity.erase<Transform, Camera>();

        // and destroy entity
        new_entity.destroy();
    }

    // you also able to remove array of Entities
    observer.destroy(); // will destroy all entities matched by Filter
}

// or you can use `world` 
auto entity = world.create();
world.emplace<Camera>(entity);
// you cannot use entity.emplace<T>() here, because World returns Entity ID
// and you have to explicitly pass it to all functions
// but you can create an empty observer and get all functionality
auto observer = Observer(world);
```

### Archetypes

You can use `Archetype` to pack components.

```cpp
struct Player {};

struct Boss {};

struct Damage {
    int damage = 0;
};

struct HP {
    int hp = 0;
};

struct Name {
    std::string name;
};

using PlayerArchetype = Archetype<Name, HP, Damage, Player>;
struct PlayerType : PlayerArchetype {
    PlayerType() : PlayerArchetype({"Player"}, {100}, {3}) {};
};

using BossArchetype = Archetype<Name, HP, Damage, Boss>;
struct BossType : BossArchetype {
    BossType() : BossArchetype({"Boss"}, {1000}, {10}) {};
};
```

You can also use it as a filter.

```cpp
using PlayerFilter = Filter<Require<PlayerArchetype>>;
using BossFilter   = Filter<Require<BossArchetype>>;
```

And create entity with all components in one line.

```cpp
void MySystem::create(OBSERVER(PlayerFilter) observer) {
    // just Archetype with default values of components
    observer.create<PlayerArchetype>();

    // or a new entity with PlayerType defaults
    observer.create<PlayerType>();

    // or exact instance
    PlayerType instance;
    instance.name = "John";
    instance.damage = 42;
    observer.create(std::move(instance));
}
```

## Multithreading

If you want to use ECS in separate thread toy can use `Registry` functions

* `syncWithRender()` - ECS function. Wait untill render calls `frameSynchronized()` function
* `frameSynchronized()` - Render functions. Call to notify ECS that frame is sinchronized and it can process the next one.
* `waitFrame()` - Render functions. Wait until ECS if processing the frame.
* `exec()` ECS function. At the end of the frame calculation sets the flag for `waitFrame()` function

Example

| Render thread: | ECS thread |
| ---  | --- |
| reg.waitFrame(); | reg.syncWithRender(); |
| sync data | -wait- |
| reg.frameSynchronized(); | -wait- |
| render data | reg.exec(); |

### Run ECS Job in separate thread

You can dispatch a separate job to work in background

```cpp
struct MySystem final : BaseSystem {
    //...
private:
    ECS_JOB worker();
};

void MySystem::setup(Registry& reg) {
    using namespace std::chrono_literals;
    ECS_JOB_RUN(reg, MySystem::worker, 1s); //execute function every second until ECS_JOB_STOP
}

ECS_JOB MySystem::worker() {
    // WARN: provide exit state in the `Stop` function for proper application exit
    if (done) {
        return ECS_JOB_STOP;
    }
    return ECS_JOB_CONTINUE;
}
```

## SAST Tools

[PVS-Studio](https://pvs-studio.com/en/pvs-studio/?utm_source=website&utm_medium=github&utm_campaign=open_source) - static analyzer for C, C++, C#, and Java code.
