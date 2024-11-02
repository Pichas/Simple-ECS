# Simple-ECS

One more ECS. You can check [this](https://gitlab.com/p34ch-main/Game) repo for use case.

## How to use

### World

First step is to create a world.

```cpp
    World world;
```

### Registry

World has a registry inside. It requires to registre systems and functions. Also it has `exec` method to calculate one frame

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

Each system can have functions to modify components. You need to provide filters to get entities from world.

```cpp
struct MySystem final : BaseSystem {
    //...
private:
    using UpdateTransformFilter = Filter<Require<Transform>, Exclude<Camera>>;
    using UpdateCameraTransformFilter = Filter<Require<Transform, Camera>>;

    void updateTransform(const Observer<UpdateTransformFilter>&);
    void updateCameraTransform(const Observer<UpdateCameraTransformFilter>&);
    // you can have more than one filter
    // void update(const Observer<UpdateTransformFilter>&, const Observer<UpdateCameraTransformFilter>&);
}
```

```cpp
void MySystem::updateTransform(const Observer<UpdateTransformFilter>& observer) {
    for (auto e : observer) {
        auto& [translation, rotation, scale] = e.get<Transform>();
        translation = {0, 1, 2};
    }
}

void MySystem::updateCameraTransform(const Observer<UpdateCameraTransformFilter>& observer) {
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
// RunEveryFrame filter is for execute function every frame without entities. You can use observer to create a new entity.
void func(const Observer<RunEveryFrame>&) {}


void MySystem::setup(Registry& reg) {
    ECS_REG_FUNC(reg, MySystem::updateTransform);
    ECS_REG_FUNC(reg, MySystem::updateCameraTransform);
    // ECS_REG_FUNC(reg, MySystem::update); // same registration for all functions

    // to register external function use ECS_REG_EXTERN_FUNC macro.
    ECS_REG_EXTERN_FUNC(reg, func);
}

void MySystem::stop(Registry& reg) {
    // when you want to remove function from execution you can use
    ECS_UNREG_FUNC(reg, MySystem::updateCamera);
    ECS_UNREG_FUNC(reg, func);
}
```

### Components

You can use any type as component, but you need to register it before. To do this we have a `ComponentRegistrant` helper class. You can check `entity_debug.cpp` to get examples.

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
void MySystem::func(const Observer<Filter>& observer) {
    for (auto e : observer) {
        // you can create a new empty entity
        auto new_entity = observer.create();

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

        // NOTE: you can use Updated<T> tag to Filter only updated components

        // get by ref for modify or const ref to read only (must be in Requires and not in Exclude)
        auto& camera = m_world.get<Camera>(new_entity);
        auto* camera_ptr = m_world.tryGet<Camera>(new_entity); // can be used without restrictions

        // check if Entity has Component
        bool has_camera = m_world.has<Camera>(new_entity);

        // erase components
        new_entity.erase<Transform>();

        // and destroy entity
        observer.destroy(new_entity);
    }

    // you also able to remove array of Entities
    observer.destroy(observer); // will destroy all entities matched by Filter
}

// or you can use `world`
```cpp
    auto entity = world.create();
    world.emplace<Camera>(entity);
    // you cannot use entity.emplace<T>() here, because World returns only Entity ID
    // and you have to explicitly pass it to all functions
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
