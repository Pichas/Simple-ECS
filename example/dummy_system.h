#pragma once

#include <simple-ecs/ECS.h>

// just for testing and instantiate templates as much as possible
template<size_t Index>
struct Dummy {
    size_t dummy;
};


using DummyArchetype = Archetype<Dummy<0>, Dummy<1>>;
struct DummyType : DummyArchetype {
    DummyType() : DummyArchetype({1000}, {10}){};
};


struct DummySystem : BaseSystem {
    DummySystem(World&);
    void setup(Registry&);
    void stop(Registry& /*unused*/) override;

private:
    using FilterOne         = Filter<Require<Dummy<0>, Dummy<1>, Dummy<2>>, Exclude<Dummy<3>, Dummy<4>, Dummy<5>>>;
    using FilterDupplicated = Filter<Require<Dummy<0>, Dummy<1>, Dummy<2>>, Exclude<Dummy<3>, Dummy<4>, Dummy<5>>>;

    void f1(OBSERVER(FilterOne));
    void f2(OBSERVER(FilterDupplicated));
};
