// Batch 21B probe -- does entering Lord British's Castle through the REAL
// transition path, with the REAL shipped slot table, put three chests in the
// basement and paint them?
//
// This closes a blind spot in batch21b_chest_reset_test.cpp, which used slot
// data typed in by hand and called hydrate_interior_objects() directly. Here
// the 32 slots are the bytes lifted out of npcs.bin in the shipped resource
// pack (payload CRC 0x2065ad91, the value the firmware gates on), and the
// entry goes through load_small_map() exactly as the Enter command does.
#include "openu5/commands.h"
#include "openu5/quest_world.h"
#include "openu5/transitions.h"
#include <cstdio>
#include <vector>

using namespace openu5;
namespace {
struct Objects { std::vector<QuestObject> values; };
size_t obj_count(void *p) { return static_cast<Objects *>(p)->values.size(); }
QuestObject obj_read(void *p, size_t i) { return static_cast<Objects *>(p)->values[i]; }
bool obj_reserve(void *p, size_t n) {
    auto &v = static_cast<Objects *>(p)->values; v.reserve(v.size() + n); return true;
}
void obj_append(void *p, const QuestObject &o) { static_cast<Objects *>(p)->values.push_back(o); }
void obj_erase(void *p, size_t i) {
    auto &v = static_cast<Objects *>(p)->values; v.erase(v.begin() + ptrdiff_t(i));
}
void obj_write(void *p, size_t i, const QuestObject &o) { static_cast<Objects *>(p)->values[i] = o; }

// Verbatim from the shipped pack: npcs.bin records 512..543, location 17.
const NpcSlot kLoc17[32] = {
#include "../loc17_slots.inc"
};
} // namespace

int main() {
    GameState g{}; TurnState t{}; TravelState travel{}; CommandState commands{};
    NpcActors actors{}; NpcScanGrid scratch{}; Objects objects{};
    // Five floors, -1..3, as smallmaps.bin carries them for location 17.
    std::vector<uint8_t> tiles(5 * 1024, 68);
    MapData maps[5];
    for (int i = 0; i < 5; ++i) maps[i] = MapData{{17, int16_t(i - 1)}, tiles.data() + i * 1024, 1024};
    WorldData world{nullptr, nullptr, 0, 0, maps, 5};
    CommandContext c{g, t, travel, commands, world};
    QuestWorldServices quest{};
    quest.context = &objects; quest.count = obj_count; quest.read = obj_read;
    quest.reserve = obj_reserve; quest.append = obj_append; quest.erase = obj_erase; quest.write = obj_write;
    NpcLocationData npc_data{17, kLoc17, 32, 0};
    c.actors = &actors; c.npc_scratch = &scratch; c.npc_data = &npc_data; c.npc_data_count = 1;
    c.quest_world = &quest;
    g.party.party_size = 1; g.party.character_count = 1; g.party.characters[0].status = 'G';

    int failures = 0;
    for (int hour = 0; hour < 24; ++hour) {
        g.time.hour = hour;
        objects.values.clear();
        Command enter; enter.kind = CommandKind::Enter;
        // Drive the transition the Enter command drives.
        struct Spy { CommandContext *c; } spy{&c};
        TransitionServices s{&spy,
            [](void *p, ReloadEffect e, uint8_t id) {
                auto &sp = *static_cast<Spy *>(p);
                if (e == ReloadEffect::HydrateInterior) hydrate_interior_objects(*sp.c, id);
            },
            nullptr};
        load_small_map(g, t, travel, 17, nullptr, s);
        g.position.map.floor = -1; // klimb down to the basement
        int chests = 0;
        for (const auto &o : objects.values) if (o.chest && o.floor == -1) ++chests;
        const int t1 = quest_world_tile(c, {17, -1}, 16, 21, 68);
        const int t2 = quest_world_tile(c, {17, -1}, 17, 22, 68);
        const int t3 = quest_world_tile(c, {17, -1}, 13, 23, 68);
        const bool ok = chests == 3 && t1 == 257 && t2 == 257 && t3 == 257;
        if (!ok) { ++failures; std::printf("hour %2d: chests=%d tiles=%d/%d/%d\n", hour, chests, t1, t2, t3); }
    }
    std::printf(failures ? "PROBE FAILED at %d hours\n" : "PROBE OK: 3 chests painted as tile 257 at every hour (%d failures)\n", failures);
    return failures ? 1 : 0;
}
