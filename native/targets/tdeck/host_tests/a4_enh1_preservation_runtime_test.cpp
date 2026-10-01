// Alpha 4 A4-ENH1 (ALPHA4_UI.md section 10) -- the Original-mode preservation
// golden on the REAL AlphaRuntime.
//
//   a4_enh1_preservation_runtime <openu5-alpha1-resources.bin>
//
// Like core/tests/a4_enh1_preservation_test.cpp, this file uses only what
// existed before A4-ENH1, and its golden hash was recorded by running it
// against the unmodified tree (commit aae348ac; native/core/
// a4-enh1-golden-head.log). It walks a party across the night-time overworld
// with the trackball for 260 steps -- the live CommandContext world() turn:
// the 0x2AE8 housekeeping (poison, food), the spawn gate and the outdoor
// spawner, monsters closing in and the fights they start -- then saves and
// loads through the System Menu's memory card. After A4-ENH1 a journey on
// Difficulty Original with no cheat must replay it bit for bit.
#include "../main/alpha_runtime.h"
#include "../main/tdeck_board.h"
#include "esp_timer.h"

#include <cstdio>
#include <cstring>
#include <memory>

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

struct Fnv {
    uint64_t h = 1469598103934665603ULL;
    void byte(uint8_t b) { h = (h ^ b) * 1099511628211ULL; }
    void add(int64_t v) {
        for (int i = 0; i < 8; ++i) byte(uint8_t(uint64_t(v) >> (8 * i)));
    }
};

openu5::Position grass_square(const openu5::WorldData &w) {
    for (int y = 40; y < 220; ++y)
        for (int x = 40; x < 220; ++x) {
            // The circuit's own cells: grass, brush and the forest family
            // (tiles 5, 6, 9..15: spawn thresholds 1 and 2).
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
        f.render_pixels = true;
        f.indexed_test_tiles = true;
        rt->attach_host_test_fixture(f);
        auto &g = rt->game();
        // Open country at midnight: the spawn gate's night threshold
        // (base + 3) makes encounters frequent. The circuit is the first
        // 15 x 15 square whose edge is all open ground that grass_square()
        // finds on the real overworld, so its steps are walks, not refusals.
        g.position.map = {0, 0};
        g.position.xy = grass_square(pack->world);
        g.time.hour = 0;
        g.time.minute = 0;
        g.food = 60;
        g.gold = 100;
        g.rng.seed(0x5eed);
        g.party.character_count = g.party.party_size = 3;
        g.party.active_character = 255;
        for (int i = 0; i < 3; ++i) {
            auto &m = g.party.characters[i];
            std::snprintf(m.name, sizeof(m.name), i ? "Ally%d" : "Avatar", i);
            m.status = i == 1 ? 'P' : 'G';
            m.party_status = 0;
            m.character_class = i ? 'F' : 'A';
            m.level = 4;
            m.current_hp = m.max_hp = 400;
            m.strength = m.dexterity = m.intelligence = 25;
            m.helmet = m.armor = m.shield = m.ring = m.amulet = 255;
            m.weapon = 255;
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
    f.add(g.gold);
    f.add(g.turns_since_start);
    f.add(g.rng.get_seed());
    for (int i = 0; i < 3; ++i) {
        const auto &m = g.party.characters[i];
        f.add(m.current_hp);
        f.add(m.status);
        f.add(m.exp);
    }
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

    // Recorded on the unmodified tree (aae348ac), see the header comment.
    constexpr uint64_t kWalk = 0x95f112a8ec6bde80ULL;
    constexpr uint64_t kSaveLoad = 0x896002146cbe321eULL;

    Run r;
    Fnv walk;
    int fights = 0, combat_steps = 0;
    bool was_combat = false;
    // A square circuit, east / south / west / north, 14 cells a side.
    static constexpr RawInputKind legs[] = {RawInputKind::TrackballRight, RawInputKind::TrackballDown,
                                            RawInputKind::TrackballLeft, RawInputKind::TrackballUp};
    for (int step = 0; step < 260; ++step) {
        r.ball(legs[(step / 14) % 4]);
        const bool combat = r.mode() == UiMode::Combat || r.rt->combat_state().initialized;
        if (combat && !was_combat) ++fights;
        combat_steps += combat;
        was_combat = combat;
        hash_state(walk, r, step);
    }
    const int poison_left = r.g().party.characters[1].current_hp;

    // The device's own save and load (System Menu's memory card), then one
    // more step: what the load restored is what the next turn reads.
    Fnv saved;
    r.key('s', true);
    hash_state(saved, r, 1000);
    r.g().food = 1;
    r.key('l', true);
    hash_state(saved, r, 1001);
    r.ball(RawInputKind::TrackballRight);
    hash_state(saved, r, 1002);

    std::printf("golden walk=0x%016llx fights=%d combat_steps=%d poisoned_hp=%d turns=%lld\n",
                (unsigned long long)walk.h, fights, combat_steps, poison_left,
                (long long)r.g().turns_since_start);
    std::printf("golden save_load=0x%016llx food=%d\n", (unsigned long long)saved.h, int(r.g().food));
    check(fights >= 1, "P1", "the night walk meets at least one encounter (the live spawn path ran)");
    check(poison_left < 400, "P2", "the poisoned ally lost HP on the walk (the poison tick ran)");
    check(walk.h == kWalk, "P3", "Original: the night walk == pre-A4-ENH1 bit for bit");
    check(saved.h == kSaveLoad, "P4", "Original: Alt+S / Alt+L / the next step == pre-A4-ENH1");
    std::printf("\nA4-ENH1 preservation runtime: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
