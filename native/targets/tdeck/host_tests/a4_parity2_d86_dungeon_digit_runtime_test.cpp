// Alpha 4 A4-PARITY2 D-86 (targets/tdeck/ALPHA4_UI.md section 16) -- a digit key in a dungeon that names an
// invalid member costs NO world turn, through the REAL AlphaRuntime (real Board, raw keys only).
//
// Derivation (native/core/a4-parity2-findings/D86-FINAL.md; re-disassembled): in a dungeon every key 0x30..0x39
// goes to DUNGEON.OVL 0x06c4 (arm 0x07bc-0x07d6), which calls the shared kernel set-active routine K:0x4080 and
// then overwrites its result with 0 (`07ce mov [bp-2],ax` / `07d1 mov word ptr [bp-2],0`). The kernel returns 1
// only for "Invalid!"; the dungeon discards it, so the loop skips exactly one thing: the per-turn block 0x0c76
// (sleeper-wake rand(0,0x3f), the wanderer step, tile effects, the status redraw, K:0x2ae8 housekeeping). The
// digit itself draws no RNG. The overworld (MAINOUT 0x0c0c) and the town (TOWN 0x0ef3) do NOT discard the 1: an
// invalid digit there costs a turn -- the dungeon's forced 0 is the only one of the three.
//
// Native ended the invalid branch with `if(!g.position.map.location) r.turn();`. A dungeon session never writes
// position.map (it stays 0 = the surface), so a full OUTDOOR turn ran on the surface tile under the entrance:
// the wind roll, two minutes, the hazard roll, housekeeping, turns_since_start++, doors, actors, the spawn gate.
//
//   V1 a valid member digit in a dungeon: the active member changes and nothing else moves
//   V2 an invalid digit (beyond the party, a dead member, a sleeping member): "Invalid!" and NO state change
//      -- not the clock, not turns_since_start, not the RNG seed, not food, not HP/status, not the dungeon cell
//   V3 '0': "None!", active 0xFF, no turn (already true)
//   V4 CONTROL: the overworld's invalid digit still costs a turn (MAINOUT 0x0c39 advance_clock(2))
//   V5 CONTROL: a valid digit on the surface costs none
//
//   a4_parity2_d86_dungeon_digit_runtime <openu5-alpha1-resources.bin>
#include "a4_ui2_harness.h"
#include "openu5/quest_state.h"

using namespace a4_ui2;

namespace {
const std::vector<Member> kParty = {{"Avatar", 'G', 200}, {"Iolo", 'G', 200}, {"Shamino", 'D', 0}};
constexpr int kDeceit = 33;

struct Snap {
    int64_t turns;
    uint32_t seed;
    int year, month, day, hour, minute;
    uint16_t food;
    uint16_t hp[3];
    char status[3];
    uint8_t active;
    uint8_t dx, dy, df;
    int px, py;
};
Snap snap(Run &h) {
    auto &g = h.rt->game();
    Snap s{};
    s.turns = g.turns_since_start;
    s.seed = g.rng.get_seed();
    s.year = g.time.year;
    s.month = g.time.month;
    s.day = g.time.day;
    s.hour = g.time.hour;
    s.minute = g.time.minute;
    s.food = g.food;
    for (int i = 0; i < 3; ++i) {
        s.hp[i] = g.party.characters[i].current_hp;
        s.status[i] = g.party.characters[i].status;
    }
    s.active = g.party.active_character;
    const auto &d = h.rt->dungeon_state();
    s.dx = d.pos.x;
    s.dy = d.pos.y;
    s.df = d.pos.floor;
    s.px = g.position.xy.x;
    s.py = g.position.xy.y;
    return s;
}
bool same_but_active(const Snap &a, const Snap &b) {
    Snap x = a, y = b;
    x.active = y.active = 0;
    return std::memcmp(&x, &y, sizeof x) == 0;
}
std::string diff(const Snap &a, const Snap &b) {
    std::string r;
    if (a.turns != b.turns) r += " turns " + std::to_string(a.turns) + "->" + std::to_string(b.turns);
    if (a.seed != b.seed) r += " seed " + std::to_string(a.seed) + "->" + std::to_string(b.seed);
    if (a.minute != b.minute || a.hour != b.hour || a.day != b.day)
        r += " clock " + std::to_string(a.hour) + ":" + std::to_string(a.minute) + "->" + std::to_string(b.hour) + ":" + std::to_string(b.minute);
    if (a.food != b.food) r += " food " + std::to_string(a.food) + "->" + std::to_string(b.food);
    for (int i = 0; i < 3; ++i)
        if (a.hp[i] != b.hp[i] || a.status[i] != b.status[i]) r += " member" + std::to_string(i);
    if (a.dx != b.dx || a.dy != b.dy || a.df != b.df) r += " dungeon-cell";
    return r.empty() ? " (none)" : r;
}

/** The party at Deceit's mouth, then (E)nter by the game's own command. */
void enter_deceit(Run &h) {
    auto &g = h.rt->game();
    g.position.map = {0, 0};
    g.position.xy = {pack->location_x[kDeceit - 1], pack->location_y[kDeceit - 1]};
    set_quest_flag(g.quest, QuestFlag::Word33);
    h.key('e');
}
bool in_dungeon(Run &h) { return h.rt->dungeon_state().active && h.rt->command_context_for_test().dungeon; }

void test_all() {
    std::printf("V  a dungeon digit that names an invalid member passes no world turn\n");
    Run h(kParty);
    h.rt->game().party.characters[1].status = 'G';
    enter_deceit(h);
    check(in_dungeon(h), "V0 control: (E)nter at Deceit's mouth opens the real dungeon session");
    const Snap start = snap(h);

    // V1: a valid member
    h.key('2');
    const Snap v1 = snap(h);
    check(v1.active == 1 && same_but_active(start, v1), "V1 a valid digit in a dungeon selects the member and moves nothing else:" + diff(start, v1));

    // V2: invalid digits
    struct Case { const char *what; char key; int member; char status; };
    const Case cases[] = {
        {"beyond the party ('5' of a party of 3)", '5', -1, 0},
        {"another beyond-the-party digit ('9')", '9', -1, 0},
        {"a dead member ('3': Shamino 'D')", '3', -1, 0},
        {"a sleeping member ('2' with Iolo 'S')", '2', 1, 'S'},
    };
    for (const auto &c : cases) {
        auto &g = h.rt->game();
        const char saved = c.member >= 0 ? g.party.characters[size_t(c.member)].status : 0;
        if (c.member >= 0) g.party.characters[size_t(c.member)].status = c.status;
        const Snap before = snap(h);
        h.key(uint8_t(c.key));
        const Snap after = snap(h);
        if (c.member >= 0) g.party.characters[size_t(c.member)].status = saved;
        check(in_dungeon(h) && std::memcmp(&before, &after, sizeof before) == 0,
              std::string("V2 an invalid digit, ") + c.what + ", changes nothing (no clock, no turn, no RNG, no food, no HP):" + diff(before, after));
    }
    // V3: '0'
    {
        const Snap before = snap(h);
        h.key('0');
        const Snap after = snap(h);
        check(after.active == 255 && same_but_active(before, after), "V3 '0' clears the active member and costs no turn:" + diff(before, after));
    }

    // V4 / V5: the surface controls
    Run s(kParty);
    s.rt->game().party.characters[1].status = 'G';
    auto &g = s.rt->game();
    g.position.map = {0, 0};
    g.position.xy = {80, 80};
    s.run(50);
    const Snap a0 = snap(s);
    s.key('2');
    const Snap a1 = snap(s);
    check(a1.active == 1 && a1.turns == a0.turns && a1.minute == a0.minute, "V5 control: a valid digit on the surface costs no turn:" + diff(a0, a1));
    s.key('9');
    const Snap a2 = snap(s);
    check(a2.turns == a1.turns + 1 && a2.seed != a1.seed && (a2.minute != a1.minute || a2.hour != a1.hour),
          "V4 control: the overworld's invalid digit still costs a turn (MAINOUT 0x0c39: advance_clock(2) + the world turn):" + diff(a1, a2));
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: a4_parity2_d86_dungeon_digit_runtime <openu5-alpha1-resources.bin>\n");
        return 2;
    }
    if (!load_pack(argv[1])) {
        std::printf("RED the pack does not load -- the checks cannot run\n");
        return 1;
    }
    static std::vector<DungeonArena> arenas;
    for (size_t i = 0; i < pack->combat_map_count; ++i) arenas.push_back({pack->combat_map_views[i], pack->combat_sprites + i * 16});
    g_arenas = arenas.data();
    g_arena_count = arenas.size();
    test_all();
    std::printf("A4-PARITY2 D-86 dungeon digit runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
