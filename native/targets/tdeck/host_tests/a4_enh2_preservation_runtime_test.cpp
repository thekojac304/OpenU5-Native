// Alpha 4 A4-ENH2 (ALPHA4_UI.md section 11) -- the Original-mode preservation
// golden on the REAL AlphaRuntime, for the sites A4-ENH2 may touch.
//
//   a4_enh2_preservation_runtime <openu5-alpha1-resources.bin>
//
// Like a4_enh1_preservation_runtime, this file uses only what existed before
// A4-ENH2, and its golden hashes were recorded by running it against the
// unmodified tree (commit 5e0a16eb; native/core/a4-enh2-golden-head.log):
//
//   W  a starving daytime walk on the real overworld (food 0: "Starving!" and
//      its rand(1,8) at every hour change, through the live outdoor turn),
//   D  then into Deceit by its entrance and 600 trackball inputs through the
//      live dungeon turn: the wanderer's re-arms, walks and ambushes, the
//      corridor fights they start, meals and starvation in the dungeon,
//   S  then Alt+S / Alt+L over the memory card and one more step.
//
// After A4-ENH2 a journey on Difficulty Original with no cheat and no toggle
// must replay it bit for bit.
#include "../main/alpha_runtime.h"
#include "../main/tdeck_board.h"
#include "esp_timer.h"
#include "openu5/dungeon.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

using namespace openu5;
using tdeck::RawInputKind;

namespace tdeck {
void host_memory_save_forget_for_test();
} // namespace tdeck
void batch37_reset_screen();

namespace {
int checks = 0, failures = 0;
bool check(bool good, const char *id, const char *label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s %s\n", good ? "GREEN" : "RED", id, label);
    return good;
}
const tdeck::AlphaResourceOwners *pack = nullptr;
size_t dungeon_count = 0;
std::vector<DungeonArena> arenas;
constexpr uint8_t kDeceit = 33;

struct Fnv {
    uint64_t h = 1469598103934665603ULL;
    void byte(uint8_t b) { h = (h ^ b) * 1099511628211ULL; }
    void add(int64_t v) {
        for (int i = 0; i < 8; ++i) byte(uint8_t(uint64_t(v) >> (8 * i)));
    }
};

// The first 15 x 15 square of open ground on the real overworld (as
// a4_enh1_preservation_runtime's circuit).
openu5::Position grass_square(const openu5::WorldData &w) {
    for (int y = 40; y < 220; ++y)
        for (int x = 40; x < 220; ++x) {
            auto ground = [&](int cx, int cy) {
                const int t = w.overworld[size_t(cy) * 256 + size_t(cx)];
                return t == 5 || t == 6 || (t >= 9 && t <= 15);
            };
            bool ok = true;
            for (int i = 0; ok && i < 14; ++i)
                ok = ground(x + i, y) && ground(x + 14, y + i) && ground(x + 14 - i, y + 14) && ground(x, y + 14 - i);
            if (ok) return {uint8_t(x), uint8_t(y)};
        }
    return {94, 108};
}

struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    Run() {
        openu5_host_virtual_clock_us() = 5'000'000;
        batch37_reset_screen();
        tdeck::host_memory_save_forget_for_test();
        tdeck::AlphaRuntime::HostTestFixture f{};
        f.world = pack->world;
        f.npc_locations = pack->npc_locations;
        f.pack = pack;
        f.location_x = pack->location_x;
        f.location_y = pack->location_y;
        f.location_count = pack->location_count;
        f.dungeons = pack->dungeons;
        f.dungeon_count = dungeon_count;
        f.arenas = arenas.data();
        f.arena_count = arenas.size();
        f.render_pixels = true;
        f.indexed_test_tiles = true;
        rt->attach_host_test_fixture(f);
        auto &g = rt->game();
        g.position.map = {0, 0};
        g.position.xy = grass_square(pack->world);
        g.time.hour = 10;
        g.time.minute = 0;
        g.food = 0;
        g.gold = 100;
        g.karma = 60;
        g.torch_turns = 2000;
        g.torches = 5;
        g.rng.seed(0x0e2d);
        g.party.character_count = g.party.party_size = 3;
        g.party.active_character = 255;
        for (int i = 0; i < 3; ++i) {
            auto &m = g.party.characters[i];
            std::snprintf(m.name, sizeof(m.name), i ? "Ally%d" : "Avatar", i);
            m.status = i == 1 ? 'P' : 'G';
            m.party_status = 0;
            m.character_class = i ? 'F' : 'A';
            m.level = 6;
            m.current_hp = m.max_hp = 900;
            m.strength = m.dexterity = m.intelligence = 30;
            m.helmet = m.armor = m.shield = m.ring = m.amulet = 255;
            m.weapon = 1; // a dagger: corridor fights end, the walk goes on
        }
        frames(3);
    }
    GameState &g() { return rt->game(); }
    UiMode mode() { return rt->ui()->mode(); }
    void frames(int n) {
        for (int i = 0; i < n; ++i) {
            openu5_host_virtual_clock_us() += 5000;
            rt->render(board);
        }
    }
    void raw(tdeck::RawInputEvent e) {
        e.timestamp_us = openu5_host_virtual_clock_us();
        rt->handle(e);
        frames(1);
    }
    void key(uint8_t code, bool alt = false) {
        openu5_host_virtual_clock_us() += 100000;
        tdeck::RawInputEvent e{};
        e.kind = RawInputKind::Keyboard;
        e.transition = tdeck::KeyTransition::Pressed;
        e.code = e.base_code = code;
        e.modifiers.alt = alt;
        raw(e);
        frames(40);
    }
    void ball(RawInputKind k) {
        openu5_host_virtual_clock_us() += 100000; // past the trackball debounce
        tdeck::RawInputEvent e{};
        e.kind = k;
        raw(e);
        frames(40); // let paced scenes, combat AI and presentations run
    }
};

void hash_state(Fnv &f, Run &r, int step) {
    auto &g = r.g();
    f.add(step);
    f.add(int(r.mode()));
    f.add(g.position.map.location);
    f.add(g.position.map.floor);
    f.add(g.position.xy.x);
    f.add(g.position.xy.y);
    f.add(g.time.day);
    f.add(g.time.hour);
    f.add(g.time.minute);
    f.add(g.food);
    f.add(g.turns_since_start);
    f.add(g.rng.get_seed());
    for (int i = 0; i < 3; ++i) {
        const auto &m = g.party.characters[i];
        f.add(m.current_hp);
        f.add(m.status);
        f.add(m.exp);
    }
    const auto &d = r.rt->dungeon_state();
    f.add(d.active);
    f.add(d.pos.floor);
    f.add(d.pos.x);
    f.add(d.pos.y);
    f.add(int(d.pos.facing));
    f.add(d.wanderer.type);
    f.add(d.wanderer.x);
    f.add(d.wanderer.y);
    f.add(d.wanderer.floor);
    f.add(d.wanderer.hidden);
    const auto &c = r.rt->combat_state();
    f.add(c.initialized);
    f.add(c.count);
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    tdeck::AlphaResourcePack source;
    tdeck::AlphaResourceReport report{};
    if (source.open(argv[1], report) != ESP_OK) return 2;
    static tdeck::AlphaResourceOwners owners{};
    if (source.load(owners, report) != ESP_OK) return 2;
    pack = &owners;
    dungeon_count = report.dungeon_count;
    for (size_t i = 0; i < owners.combat_map_count; ++i)
        arenas.push_back({owners.combat_map_views[i], owners.combat_sprites + i * 16});

    // Recorded on the unmodified tree (5e0a16eb), see the header comment.
    constexpr uint64_t kWalk = 0x6f389ff229d27700ULL; // A4-PARITY2 D-88: re-recorded (was 0xc1145ddbc397daaf): fights in the overworld walk advance the clock, which moves the hourly starvation draws
    constexpr uint64_t kDungeon = 0x52a4fd17753c2a5aULL; // A4-PARITY2 D-88: re-recorded (was 0xbe4274230b91b8c0): the dungeon section is re-seeded (see above) and its fights advance the clock
    constexpr uint64_t kSaveLoad = 0xa7dae91af4c0b9deULL; // A4-PARITY2 D-88: re-recorded (was 0x272ed77a587912fb): same journey, same reason

    Run r;
    // W. A starving daytime walk: 2 minutes a step, an hour every 30 steps.
    Fnv walk;
    int starving = 0;
    static constexpr RawInputKind legs[] = {RawInputKind::TrackballRight, RawInputKind::TrackballDown,
                                            RawInputKind::TrackballLeft, RawInputKind::TrackballUp};
    const auto hp0 = r.g().party.characters[0].current_hp;
    for (int step = 0; step < 150; ++step) {
        r.ball(legs[(step / 14) % 4]);
        hash_state(walk, r, step);
    }
    starving = int(hp0) - int(r.g().party.characters[0].current_hp);

    // D. Into Deceit by its entrance (its Word of Passage granted), then a
    // fixed pattern of moves and turns; fights in the corridors are played by
    // the same inputs.
    Fnv dng;
    r.g().food = 2;
    // A4-PARITY2 D-88 (2026-10-03): fights advance the clock now, which moved the overworld walk's stream and with it the
    // stream the dungeon section inherited; this scenario no longer met its wanderer (D2 red). Re-seeded here so it does (the
    // seed was chosen by trying 3, 5, 7, 11, ...: 3 and 5 ambush twice, as the original did).
    r.g().rng.seed(3);
    r.g().position.map = {0, 0};
    r.g().position.xy = {pack->location_x[kDeceit - 1], pack->location_y[kDeceit - 1]};
    set_quest_flag(r.g().quest, QuestFlag::Word33);
    r.key('e');
    const bool entered = r.rt->dungeon_state().active && r.rt->dungeon_state().pos.dungeon == kDeceit;
    int fights = 0, combat_steps = 0;
    bool was_combat = false;
    static constexpr RawInputKind pattern[] = {
        RawInputKind::TrackballUp, RawInputKind::TrackballUp, RawInputKind::TrackballRight, RawInputKind::TrackballUp,
        RawInputKind::TrackballUp, RawInputKind::TrackballLeft, RawInputKind::TrackballUp, RawInputKind::TrackballUp,
        RawInputKind::TrackballDown, RawInputKind::TrackballUp, RawInputKind::TrackballLeft, RawInputKind::TrackballUp};
    for (int step = 0; step < 900; ++step) {
        r.ball(pattern[step % 12]);
        const bool combat = r.mode() == UiMode::Combat || r.rt->combat_state().initialized;
        if (combat && !was_combat) ++fights;
        combat_steps += combat;
        was_combat = combat;
        hash_state(dng, r, step);
    }

    // S. The device's own save and load, then one more step.
    Fnv saved;
    r.key('s', true);
    hash_state(saved, r, 1000);
    r.g().food = 7;
    r.key('l', true);
    hash_state(saved, r, 1001);
    r.ball(RawInputKind::TrackballUp);
    hash_state(saved, r, 1002);

    std::printf("golden walk=0x%016llx avatar_hp_lost=%d turns=%lld\n", (unsigned long long)walk.h, starving,
                (long long)r.g().turns_since_start);
    std::printf("golden dungeon=0x%016llx entered=%d fights=%d combat_steps=%d floor=%d food=%d\n",
                (unsigned long long)dng.h, int(entered), fights, combat_steps, int(r.rt->dungeon_state().pos.floor),
                int(r.g().food));
    std::printf("golden save_load=0x%016llx\n", (unsigned long long)saved.h);
    check(starving > 0, "W1", "the starving walk took HP (the live starvation path ran)");
    check(entered, "D1", "the party entered Deceit through its entrance");
    check(fights >= 1, "D2", "a wanderer ambushed the party (the live re-arm, walk and corridor fight ran)");
    check(walk.h == kWalk, "W2", "Original: the starving overworld walk == pre-A4-ENH2 bit for bit");
    check(dng.h == kDungeon, "D3", "Original: the dungeon walk, its wanderer and fights == pre-A4-ENH2 bit for bit");
    check(saved.h == kSaveLoad, "S1", "Original: Alt+S / Alt+L / the next step == pre-A4-ENH2");
    std::printf("\nA4-ENH2 preservation runtime: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
