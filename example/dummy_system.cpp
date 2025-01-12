#include "dummy_system.h"
#include <simple-ecs/ECS.h>


DummySystem::DummySystem(World& w) {
    [&]<size_t... Is>(std::index_sequence<Is...>) {
        ComponentRegistrant<Dummy<Is>...>(w).createStorage();
        auto observer = Observer(w);
        auto e        = observer.create();
        e.emplaceTagged<Dummy<Is>...>();
    }(std::make_index_sequence<32>{});
}

void DummySystem::setup(Registry& reg) {
    ECS_REG_FUNC(reg, DummySystem::f1);
    ECS_REG_FUNC(reg, DummySystem::f2);
}

void DummySystem::stop(Registry& reg) {
    ECS_UNREG_FUNC(reg, DummySystem::f1);
    ECS_UNREG_FUNC(reg, DummySystem::f2);
}

void DummySystem::f1([[maybe_unused]] OBSERVER(FilterOne) observer) {

#if 1
    auto f = [&]<EcsTarget Target>(Target target) {
        observer.destroy(target);

        std::ignore = observer.isAlive(target);
        std::ignore = observer.has<Dummy<0>>(target);

        observer.emplace<Dummy<0>>(target, 12);
        observer.emplace<Dummy<0>, Dummy<1>, Dummy<2>>(target);
        observer.emplace(target, Dummy<0>{}, Dummy<1>{}, Dummy<2>{});

        observer.emplaceTagged<Dummy<0>>(target, 12);
        observer.emplaceTagged<Dummy<0>, Dummy<1>, Dummy<2>>(target);
        observer.emplaceTagged(target, Dummy<0>{}, Dummy<1>{}, Dummy<2>{});

        observer.markUpdated<Dummy<0>, Dummy<1>, Dummy<2>>(target);
        observer.clearUpdateTag<Dummy<0>, Dummy<1>, Dummy<2>>(target);

        observer.markUpdated<Dummy<0>, Dummy<1>, Dummy<2>>();
        observer.clearUpdateTag<Dummy<0>, Dummy<1>, Dummy<2>>();


        observer.erase<Dummy<0>, Dummy<1>, Dummy<2>>(target);
        observer.erase<Dummy<0>, Dummy<1>, Dummy<2>>();
    };

    for (auto e : observer) {
        assert(observer.size() == 1);

        Entity                  ent  = e;
        std::span<const Entity> span = observer;

        f(ent);
        f(span);

        std::ignore = observer.getRegistry();
        std::ignore = observer.begin();
        std::ignore = observer.end();
        std::ignore = observer.size();
        std::ignore = observer.empty();
        std::ignore = observer.entities();
        std::ignore = observer.create();
        std::ignore = observer.create<DummyArchetype>();
        std::ignore = observer.create<DummyType>();
        std::ignore = observer.create(DummyArchetype());
        std::ignore = observer.create(DummyType());
        std::ignore = observer.get<Dummy<1>>(ent);
        std::ignore = observer.tryGet<Dummy<12>>(ent);
        std::ignore = observer[0];
        observer.destroy();

        //---

        std::ignore = e.isAlive();
        std::ignore = e.get();
        std::ignore = e.has<Dummy<1>>();
        std::ignore = e.get<Dummy<1>>();
        std::ignore = e.tryGet<Dummy<12>>();

        e.emplace<Dummy<0>, Dummy<1>, Dummy<2>>();
        e.emplace(Dummy<0>{}, Dummy<1>{}, Dummy<2>{});

        e.emplaceTagged<Dummy<0>, Dummy<1>, Dummy<2>>();
        e.emplaceTagged(Dummy<0>{}, Dummy<1>{}, Dummy<2>{});

        e.markUpdated<Dummy<0>, Dummy<1>, Dummy<2>>();
        e.clearUpdateTag<Dummy<0>, Dummy<1>, Dummy<2>>();

        e.erase<Dummy<0>, Dummy<1>, Dummy<2>>();

        e.destroy();


        auto [a, b, c] = e.get();

        if (a.dummy && b.dummy && c.dummy) {
            spdlog::info("ref");
        }
    }

#endif
};

void DummySystem::f2(OBSERVER(FilterDupplicated) observer) {
    for (const auto& e : observer) {
        auto [a, b, c] = e.get();

        if (a.dummy && b.dummy && c.dummy) {
            spdlog::info("const ref");
        }
    }
}
