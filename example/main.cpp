#include "components.h"
#include <simple-ecs/ECS.h>
#include "battle_system.h"
#include "dummy_system.h"
#include "hp_system.h"
#include <ct/random.h>


int main() {
#if defined(_WIN32) && defined(_DEBUG)
    // memleak detection. Debug build only
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    spdlog::default_logger()->set_level(spdlog::level::debug);

    World w;

    ComponentRegistrant<Dead, Player, Boss>(w).createStorage();
    ComponentRegistrant<Damage, HP>(w).createStorage();

    ComponentRegistrant<Player>(w)
      .addEmplaceCallback([](Entity e) { spdlog::debug("Entity {} with Tag {} was created", e, ct::name_sv<Player>); })
      .addDestroyCallback([](Entity e) { spdlog::debug("Entity {} with Tag {} was removed", e, ct::name_sv<Player>); });

    ComponentRegistrant<HP>(w).addDestroyCallback([&w](Entity e, HP& c) {
        const auto& name = w.get<Name>(e);
        spdlog::debug("Entity '{}', Name '{}' with HP '{}' was removed", e, name.name, c.hp);
        c.hp = 0; // do something with component before destroy
    });

    auto* reg = w.getRegistry();
    reg->addSystem<HPSystem>();
    reg->addSystem<BattleSystem>();
    reg->addSystem<DummySystem>(w);
    reg->initNewSystems();


    auto observer = Observer(w);

    for (int i = 0; i < 6; i++) {
        PlayerType instance;
        instance.name = "Player " + std::to_string(i);
        observer.create(std::move(instance));
    }

    BossType instance;
    instance.name = "Boss";
    observer.create(std::move(instance));


    while (true) {
        ECS_PROFILER(FrameMark);

        reg->prepare();
        reg->exec();

        spdlog::info("--- Players {}, Bosses {} ---", w.size<Player>(), w.size<Boss>());
        if (w.empty<Player>() || w.empty<Boss>()) {
            break;
        }

        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1ms);
    }

    // will be removed anyway, but just
    // to check that all works fine
    reg->removeSystem<HPSystem>();
    reg->removeSystem<BattleSystem>();

    // one more iteration to apply changes
    reg->prepare();
    reg->exec();

    return 0;
}