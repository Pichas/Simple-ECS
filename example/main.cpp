#include "components.h"
#include <simple-ecs/ECS.h>
#include "hp_system.h"
#include <ct/random.h>
#include <iostream>


int main() {

    // // std::vector<DEBUG_CMP> v;
    // // v.resize(1);
    // // v.resize(5);

    // // return 0;
    // // std::vector<std::byte> mem;
    // // mem.resize(sizeof(std::string));
    // void* mem = std::malloc(sizeof(DEBUG_CMP));
    // // auto* p = reinterpret_cast<std::string*>(mem.data());

    // // char* mem[sizeof(std::string)];
    // auto* p = reinterpret_cast<DEBUG_CMP*>(mem);
    // spdlog::info("{:x}", (size_t)std::launder(p));

    // std::construct_at(std::launder(p), 12);


    // void* mem2 = std::realloc(mem, sizeof(DEBUG_CMP) * 2);
    // // std::memmove(mem2, mem, sizeof(std::string));
    // p = reinterpret_cast<DEBUG_CMP*>(mem2);

    // // mem.resize(sizeof(std::string) * 2);
    // // p = reinterpret_cast<std::string*>(mem);

    // spdlog::info("{:x}", (size_t)std::launder(p));
    // // spdlog::info("{}", std::launder(p)->c_str());
    // std::destroy_at(std::launder(p));
    // std::free(mem2);

    // return 0;
    World w;

    ComponentRegistrant<Dead, Damage, PlayerName, HP>(w).createStorage();

    auto* reg = w.getRegistry();
    reg->addSystem<HPSystem>();
    reg->initNewSystems();

    auto e = w.create();
    w.emplace<HP>(e, 2);

    while (!w.empty()) {
        reg->exec();
        if (!dice(0, 100)) {
            if (w.isAlive(e)) {
                w.emplace<Damage>(e, 1);
            }
        }
    }

    return 0;
}