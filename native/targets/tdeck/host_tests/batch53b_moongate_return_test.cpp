// Batch 53B -- the return trip through a moongate, on the device's own runtime.
//
// Phase 7E-C on the Batch 53 image: Developer teleport to Britannia 96,103,
// hour 21; the gate is drawn one cell North; stepping North onto it transits
// (PASS). At the destination the gate is still drawn, but stepping back onto
// it "does not transport the party back". Vas Rel Por PASS.
//
// The original (re/notes/batch53b-moongate-return.md, from the shipped bytes):
//
//   * The outdoor main loop (MAINOUT.OVL 0x0a84, loaded at 0x81d0) calls
//     kernel_moongate_enter (ULTIMA.EXE 0x48a8) at the top of EVERY
//     iteration (MAINOUT 0x0b00), before the key read. There is no skip flag
//     outdoors; the only "just arrived" guard in the binary is the town
//     loop's one-shot [0xa9bc] (TOWN.OVL 0x1468, set by the town loader
//     0x11f0), which never runs on the surface.
//   * 0x48a8 fires when the MAP BUFFER cell under the party is 0xDC -- the
//     tile kernel_moongate_render (0x475a) paints on every buried stone of
//     this location at night. The same buffer is the only thing that makes a
//     gate visible, so "visible" and "active" are one predicate.
//   * The destination is a function of the CLOCK ONLY: 0x4962 picks
//     g_felucca_phase before noon, g_trammel_phase after, minus '0', and
//     0x4977 calls kernel_moongate_teleport (0x47f4) with it, which copies
//     that stone's x/y/location/floor. Nothing in 0x48a8 / 0x47f4 reads the
//     origin gate, and neither advances the clock (advance_clock 0x4f7c is
//     only called with 0 on that path, which returns at once).
//
// So every gate in the world leads to the SAME stone during one phase: the
// one the party just arrived on. Stepping back onto the destination gate at
// the same hour transits the party onto itself (on the original the gate
// closes and re-opens around the party; on the device, which has no transit
// animation yet (Alpha 3), nothing visible happens). A "return trip" exists
// only once the moons select the origin stone's phase (or by Vas Rel Por).
//
// This file drives that exact physical sequence on the device runtime and
// separates the three hypotheses:
//   "re-entry blocked by design"  -> the gate would NOT fire (no sfx);
//   "broken second transit"       -> it would not fire, or land elsewhere;
//   "original semantics"          -> it FIRES, and lands where it stands.
//
// Seam: the Batch 11 host fixture over the shipped pack (production binders);
// every action is a RawInputEvent through AlphaRuntime::handle(). The one
// observation the transcript cannot carry -- "did the gate fire?" -- is read
// from the core's own event stream by wrapping the runtime's EventSink and
// forwarding every event unchanged.
#include "../main/alpha_runtime.h"
#include "../main/tdeck_board.h"

#include "esp_timer.h"
#include "openu5/debug_developer.h"
#include "openu5/dungeon.h"
#include "openu5/presentation.h"
#include "openu5/quest_world.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

using namespace openu5;
using tdeck::RawInputKind;

namespace {

int g_failures = 0, g_checks = 0;
bool expect(bool ok, const char *id, const char *what) {
    ++g_checks;
    std::printf("  %-5s %-6s %s\n", ok ? "GREEN" : "RED", id, what);
    if (!ok) ++g_failures;
    return ok;
}

const tdeck::AlphaResourceOwners *g_owners = nullptr;
tdeck::AlphaResourceReport g_report{};
std::vector<DungeonArena> g_arenas;

// Counts the gate's own activation cue (commands.cpp: Sfx "moongate", emitted
// once per kernel_moongate_enter that passes the gate test) and forwards
// every event to the runtime's sink untouched.
struct GateSpy {
    EventSink inner{};
    int fired = 0;
    static void emit(void *p, const GameEvent &e) {
        auto &s = *static_cast<GateSpy *>(p);
        if (e.kind == GameEventKind::Sfx && e.text && std::strcmp(e.text, "moongate") == 0) ++s.fired;
        if (s.inner.emit) s.inner.emit(s.inner.context, e);
    }
};

struct Harness {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    size_t mark = 0;
    GateSpy spy;
    Harness() {
        tdeck::AlphaRuntime::HostTestFixture hf;
        hf.world = g_owners->world;
        hf.location_x = g_owners->location_x; hf.location_y = g_owners->location_y;
        hf.location_count = g_owners->location_count;
        hf.npc_locations = g_owners->npc_locations;
        hf.dungeons = g_owners->dungeons; hf.dungeon_count = g_report.dungeon_count;
        hf.arenas = g_arenas.data(); hf.arena_count = g_arenas.size();
        hf.enemy_defs = g_owners->combat_enemy_views; hf.enemy_def_count = g_owners->combat_enemy_count;
        hf.pack = g_owners;
        rt->attach_host_test_fixture(hf);
        auto &g = rt->game();
        g.party.character_count = g.party.party_size = 1;
        auto &ch = g.party.characters[0];
        std::snprintf(ch.name, sizeof(ch.name), "Avatar");
        ch.current_hp = ch.max_hp = 900; ch.status = 'G'; ch.party_status = 0; ch.character_class = 'A';
        ch.dexterity = 30; ch.strength = 30; ch.intelligence = 30; ch.level = 8; ch.current_mp = 99;
        g.party.active_character = 0;
        g.time.hour = 12; g.time.minute = 0;
        g.torch_turns = 500; g.torches = 5; g.karma = 50; g.gold = 321; g.food = 900;
        g.position.map = {0, 0};
        g.transport = TransportMode::Foot; rt->turn().transport_tile = 0x1c;
        spy.inner = ctx().events;
        ctx().events = {&spy, GateSpy::emit};
    }
    GameState &g() { return rt->game(); }
    CommandContext &ctx() { return rt->command_context_for_test(); }
    UiMode mode() const { return rt->ui()->mode(); }
    const QuestWorldServices *quest() { return ctx().quest_world; }

    static void advance(int64_t us) { openu5_host_virtual_clock_us() += us; }
    bool key(uint8_t code) {
        advance(100000);
        tdeck::RawInputEvent raw{};
        raw.kind = RawInputKind::Keyboard; raw.code = code;
        raw.transition = tdeck::KeyTransition::Pressed; raw.timestamp_us = openu5_host_virtual_clock_us();
        return rt->handle(raw);
    }
    bool ball(RawInputKind kind) {
        advance(100000);
        tdeck::RawInputEvent raw{}; raw.kind = kind; raw.timestamp_us = openu5_host_virtual_clock_us();
        return rt->handle(raw);
    }
    void step(Direction d) {
        ball(d == Direction::North ? RawInputKind::TrackballUp : d == Direction::South ? RawInputKind::TrackballDown
             : d == Direction::East ? RawInputKind::TrackballRight : RawInputKind::TrackballLeft);
        // A4-UI4 (D-48): a gate's transit (kernel 0x48a8) reads no key for its
        // ~3 s; a player waits it out, and so does this harness (which renders no
        // frame: the transit's hold is on its own clock).
        if (rt->moongate_transit_active()) advance(3200000);
    }
    void set_mark() { mark = rt->ui()->transcript_size(); }
    bool saw(const char *needle) const {
        for (size_t i = mark; i < rt->ui()->transcript_size(); ++i)
            if (const auto *b = rt->ui()->transcript_at(i); b && std::strstr(b->text, needle)) return true;
        return false;
    }
    void dump(const char *tag) const {
        std::printf("         transcript since mark (%s):\n", tag);
        for (size_t i = mark; i < rt->ui()->transcript_size(); ++i)
            if (const auto *b = rt->ui()->transcript_at(i)) std::printf("           | %s\n", b->text);
    }
    int x() { return g().position.xy.x; }
    int y() { return g().position.xy.y; }
    bool at(int px, int py) { return g().position.map.location == 0 && g().position.map.floor == 0 && x() == px && y() == py; }
    int32_t tile(int tx, int ty) {
        return quest_world_tile(ctx(), g().position.map, tx, ty, get_active_map(ctx().world, g().position.map).value.tile_at(tx, ty));
    }
    // What the device composes at (tx, ty): the same call the renderer makes.
    int composed(int tx, int ty) {
        const auto map = get_active_map(ctx().world, g().position.map);
        const auto s = compose_world_presentation(ctx(), map.value, g().position.xy, 0x11c);
        const int col = tx - x() + kPresentationWindow / 2, row = ty - y() + kPresentationWindow / 2;
        if (col < 0 || row < 0 || col >= kPresentationWindow || row >= kPresentationWindow) return -1;
        return s.tiles[row * kPresentationWindow + col];
    }
    int phase() { return active_gate_phase(g(), rt->turn(), *quest()); }
};

constexpr Direction kDirs[] = {Direction::South, Direction::North, Direction::East, Direction::West};
Direction opposite(Direction d) {
    return d == Direction::South ? Direction::North : d == Direction::North ? Direction::South
         : d == Direction::East ? Direction::West : Direction::East;
}
const char *dir_name(Direction d) {
    return d == Direction::North ? "North" : d == Direction::South ? "South" : d == Direction::East ? "East" : "West";
}
// Walkable (on foot) neighbours of a surface cell, as the direction to step
// FROM the cell onto them.
std::vector<Direction> free_sides(Harness &h, int cx, int cy) {
    std::vector<Direction> out;
    for (auto d : kDirs) {
        const auto dd = direction_delta(d);
        const int t = h.tile((cx + dd.dx) & 255, (cy + dd.dy) & 255);
        if (t >= 0 && is_passable(t, TransportMode::Foot).value) out.push_back(d);
    }
    return out;
}
std::vector<Direction> blocked_sides(Harness &h, int cx, int cy) {
    std::vector<Direction> out;
    for (auto d : kDirs) {
        const auto dd = direction_delta(d);
        const int t = h.tile((cx + dd.dx) & 255, (cy + dd.dy) & 255);
        if (t >= 0 && !is_passable(t, TransportMode::Foot).value) out.push_back(d);
    }
    return out;
}
bool dev_teleport(Harness &h, int x, int y) {
    DebugTeleportRequest r;
    r.kind = DebugDestinationKind::Britannia; r.location = 0; r.floor = 0; r.x = x; r.y = y; r.standard_entry = false;
    return apply_debug_teleport(h.ctx(), r).status == DebugTeleportStatus::Applied;
}

// INIT.GAM's stones, from the pack (0x28a x, 0x292 y, 0x29a location, 0x2a2 floor).
Moonstone init_stone(int i) {
    const uint8_t *gam = g_owners->initial_gam;
    return {gam[0x28a + i], gam[0x292 + i], int16_t(gam[0x2a2 + i]), gam[0x29a + i], gam[0x29a + i] != 255};
}
bool stones_are_init(Harness &h) {
    const auto *q = h.quest();
    if (!q || q->moonstone_count != 8) return false;
    for (int i = 0; i < 8; ++i) {
        const auto a = q->moonstones[i], b = init_stone(i);
        if (a.x != b.x || a.y != b.y || a.z != b.z || a.location != b.location || a.buried != b.buried) return false;
    }
    return true;
}
// The data's phase for (day, hour): DATA.OVL 0x1EEA pair minus '0',
// Felucca before noon, Trammel after (kernel 0x4962-0x4973).
int table_phase(int day, int hour) { return g_owners->moon_phases[(day - 1) * 2 + (hour < 12 ? 0 : 1)] - 48; }

// The checklist's 7E-C route, steps 1-3: Developer teleport to 96,103 (the
// cell South of stone 1, 96,102), Time -> Hour 21, move North onto the gate.
struct Arrival { bool ok = false; int phase = -1; Moonstone dest{}; };
Arrival arrive(Harness &h, bool report) {
    Arrival a;
    const auto origin = init_stone(1);
    const bool teleported = dev_teleport(h, origin.x, origin.y + 1) && h.at(origin.x, origin.y + 1);
    debug_set_clock(h.g(), DebugClockPart::Hour, 21);
    debug_set_clock(h.g(), DebugClockPart::Minute, 0);
    const int drawn = h.composed(origin.x, origin.y);
    const int before = h.spy.fired;
    h.step(Direction::North);
    a.phase = h.phase();
    a.dest = a.phase >= 0 && a.phase < 8 ? init_stone(a.phase) : Moonstone{};
    a.ok = teleported && drawn == 0xdc && h.spy.fired == before + 1 && a.phase >= 0 && h.at(a.dest.x, a.dest.y);
    if (report) {
        expect(teleported, "R1", "7E-C step 1: the Developer teleport lands at Britannia 96,103");
        expect(drawn == 0xdc, "R2", "7E-C step 2: at hour 21 the gate tile 0xDC is drawn one cell North, on stone 1 (96,102)");
        expect(a.ok, "R3", "7E-C step 3: moving North onto it fires the gate once and lands on the active phase's stone");
        std::printf("         day %d, hour 21: Trammel phase %d -> stone %d at %d,%d (origin gate = stone 1 at %d,%d)\n",
                    int(h.g().time.day), a.phase, a.phase, a.dest.x, a.dest.y, origin.x, origin.y);
    }
    return a;
}

// ===========================================================================
// R. The hardware result, reproduced on the device runtime.
// ===========================================================================
void test_reproduction() {
    std::printf("R  Phase 7E-C as run on the hardware\n");
    Harness h;
    const auto a = arrive(h, true);
    expect(a.phase != 1, "R4", "precondition: tonight's phase is not stone 1's, so the first transit really moved the party");
    const int hx = h.x(), hy = h.y();
    std::printf("         composed on the party's own cell: 0x%x (the party sprite is drawn over whatever lies beneath)\n",
                unsigned(h.composed(hx, hy)));
    expect(moongate_visible_at(h.g(), h.rt->turn(), *h.quest(), hx, hy) && moongate_at(h.g(), h.rt->turn(), *h.quest()), "R6",
           "** at the destination the draw predicate and the transit predicate agree: visible AND active **");
    const int now = h.phase();
    expect(now == a.phase && init_stone(now).x == hx && init_stone(now).y == hy, "R7",
           "** the active phase still selects the stone the party stands on: this gate's destination is itself **");

    // The tester's action: step off, step back on.
    const auto sides = free_sides(h, hx, hy);
    if (!expect(!sides.empty(), "R8", "precondition: the destination gate has a walkable neighbour")) return;
    const auto off = sides.front();
    const int before = h.spy.fired;
    h.set_mark();
    h.step(off);
    const bool left = !h.at(hx, hy) && h.spy.fired == before;
    expect(h.composed(hx, hy) == 0xdc, "R8b",
           "** one step off, the destination gate is drawn: 0xDC is composed on the stone the party arrived on **");
    h.step(opposite(off));
    const bool fired = h.spy.fired == before + 1;
    std::printf("         stepped %s off and %s back on: gate fired %d time(s); party at %d,%d\n", dir_name(off),
                dir_name(opposite(off)), h.spy.fired - before, h.x(), h.y());
    expect(left, "R9", "stepping off the destination gate is an ordinary step (no transit, no refusal)");
    if (!expect(fired, "R10", "** stepping back onto the destination gate FIRES the gate: re-entry is not blocked **"))
        h.dump("R10");
    expect(h.at(hx, hy) && !h.at(init_stone(1).x, init_stone(1).y), "R11",
           "** ...and lands where it stands (the active stone), not back at 96,102: the hardware observation, reproduced **");
    expect(!h.saw("Failed") && h.mode() == UiMode::Exploration, "R12", "no \"Failed!\", no prompt, the session stays in Exploration");
}

// ===========================================================================
// O. The original's contract for every re-entry variant.
// ===========================================================================
void test_reentry_variants() {
    std::printf("O  re-entry variants (all at the same hour and phase)\n");
    // O1: from every walkable side, not just the one the party left by.
    {
        Harness h;
        const auto a = arrive(h, false);
        const int hx = h.x(), hy = h.y();
        const auto sides = free_sides(h, hx, hy);
        int ok = 0;
        for (auto d : sides) {
            const int before = h.spy.fired;
            h.step(d);
            const bool off = !h.at(hx, hy);
            h.step(opposite(d));
            if (off && h.spy.fired == before + 1 && h.at(hx, hy)) ++ok;
        }
        std::printf("         %zu walkable side(s) of the destination gate; %d re-entries fired and landed on it\n", sides.size(), ok);
        expect(a.ok && !sides.empty() && ok == int(sides.size()), "O1",
               "** entering from ANY adjacent cell fires the gate and lands on the active stone (no previous-position state) **");
    }
    // O2: a second full lap, then a third: no anti-retrigger that latches on.
    {
        Harness h;
        const auto a = arrive(h, false);
        const int hx = h.x(), hy = h.y();
        const auto sides = free_sides(h, hx, hy);
        int laps = 0;
        for (int i = 0; i < 3 && !sides.empty(); ++i) {
            const int before = h.spy.fired;
            h.step(sides.front()); h.step(opposite(sides.front()));
            laps += h.spy.fired == before + 1 && h.at(hx, hy);
        }
        expect(a.ok && laps == 3, "O2", "** three laps off and back on: the gate fires every time (no suppression flag that never clears) **");
    }
    // O3: wait a turn standing on the gate, then step off and back on.
    {
        Harness h;
        const auto a = arrive(h, false);
        const int hx = h.x(), hy = h.y();
        const int before = h.spy.fired;
        const auto routed = h.rt->routed_command_count();
        h.key(' ');
        const int on_pass = h.spy.fired - before;
        const bool stayed = h.at(hx, hy) && h.rt->routed_command_count() > routed;
        const auto sides = free_sides(h, hx, hy);
        const int mid = h.spy.fired;
        if (!sides.empty()) { h.step(sides.front()); h.step(opposite(sides.front())); }
        std::printf("         Pass on the gate: the device fired the gate %d time(s) (the original re-fires here, to the same stone;"
                    " see the audit's declared divergence)\n", on_pass);
        expect(a.ok && stayed, "O3", "Pass on the destination gate is routed and leaves the party on the same cell");
        expect(!sides.empty() && h.spy.fired == mid + 1 && h.at(hx, hy), "O4",
               "** after waiting a turn, stepping off and back on fires the gate and lands on the active stone **");
    }
    // O5: "facing only". On foot the overworld party has no facing (the
    // transport tile is fixed); the nearest thing is a step that does not
    // move -- a bump into an impassable neighbour.
    {
        Harness h;
        const auto a = arrive(h, false);
        const int hx = h.x(), hy = h.y();
        const auto walls = blocked_sides(h, hx, hy);
        if (walls.empty()) {
            std::printf("         no impassable neighbour at the destination: the bump variant is N/A there; Pass (O3) stands in\n");
            expect(a.ok, "O5", "the party still stands on the destination gate (bump variant N/A)");
        } else {
            const int before = h.spy.fired;
            h.step(walls.front());
            expect(a.ok && h.at(hx, hy) && h.spy.fired == before, "O5",
                   "a bump that does not move the party does not fire the gate (the MOVE handler's trigger needs a step)");
        }
    }
    // O6: one step = one activation; no ping-pong after a same-cell landing.
    {
        Harness h;
        const auto a = arrive(h, false);
        const int hx = h.x(), hy = h.y();
        const auto sides = free_sides(h, hx, hy);
        const int before = h.spy.fired;
        if (!sides.empty()) h.step(sides.front());
        const int after_off = h.spy.fired;
        if (!sides.empty()) h.step(opposite(sides.front()));
        const int after_on = h.spy.fired;
        if (!sides.empty()) h.step(sides.front());
        expect(a.ok && after_off == before && after_on == before + 1 && h.spy.fired == after_on && !h.at(hx, hy), "O6",
               "** exactly one activation per step onto the gate, none on the steps off: no double trigger, no ping-pong **");
    }
    // O7: the clock does not move the destination during the lap.
    {
        Harness h;
        const auto a = arrive(h, false);
        const auto sides = free_sides(h, h.x(), h.y());
        if (!sides.empty()) { h.step(sides.front()); h.step(opposite(sides.front())); }
        expect(a.ok && h.g().time.hour == 21 && h.phase() == a.phase && stones_are_init(h), "O7",
               "same hour, same phase, and the eight stones untouched by any transit (0x47f4 only reads them)");
    }
}

// ===========================================================================
// T. The valid return trip, and the controls around it.
// ===========================================================================
void test_return_trip() {
    std::printf("T  the return trip: wait for the moons, or cast\n");
    // T1: the moons select stone 1. Day 3's Trammel byte is '1' (DATA.OVL
    // 0x1EEA). Standing on the destination gate, the Developer sets Day 3 and
    // Hour 21 (the checklist's own tool); the party steps off and back on.
    // The latch is left exactly as the device runtime holds it.
    {
        Harness h;
        const auto a = arrive(h, false);
        const auto origin = init_stone(1);
        int day = 0;
        for (int d = 1; d <= 28 && !day; ++d) if (table_phase(d, 21) == 1) day = d;
        const auto &t = h.rt->turn();
        std::printf("         device latch after arrival: felucca=%d trammel=%d (valid only in 0x30..0x37)\n",
                    int(t.felucca_phase), int(t.trammel_phase));
        debug_set_clock(h.g(), DebugClockPart::Day, day);
        debug_set_clock(h.g(), DebugClockPart::Hour, 21);
        debug_set_clock(h.g(), DebugClockPart::Minute, 0);
        const int hx = h.x(), hy = h.y();
        const bool selects_home = h.phase() == 1;
        const auto sides = free_sides(h, hx, hy);
        const int before = h.spy.fired;
        if (!sides.empty()) { h.step(sides.front()); h.step(opposite(sides.front())); }
        std::printf("         Day %d (Trammel '%d'), 21:00: after stepping off and back on the party is at %d,%d\n",
                    day, table_phase(day, 21), h.x(), h.y());
        expect(day == 3 && selects_home, "T1", "on Day 3 at hour 21 the active phase is 1 (the Britain stone's)");
        expect(a.ok && h.spy.fired == before + 1 && h.at(origin.x, origin.y), "T2",
               "** then stepping off the destination gate and back on transits to stone 1: the party is back at 96,102 **");
    }
    // Probe (not a check): the ORIGINAL latches the phases only at an hour
    // boundary on the surface (0x4a84; advance_clock -> refresh_moon_phase_latch
    // here). Seed a latch from the arrival day and cross 21:00 on Day 3: the
    // original would switch to Day 3's phases at the boundary.
    {
        Harness h;
        arrive(h, false);
        auto &t = h.rt->turn();
        const int d0 = h.g().time.day;
        t.felucca_phase = g_owners->moon_phases[(d0 - 1) * 2]; t.trammel_phase = g_owners->moon_phases[(d0 - 1) * 2 + 1];
        debug_set_clock(h.g(), DebugClockPart::Day, 3);
        debug_set_clock(h.g(), DebugClockPart::Hour, 20);
        debug_set_clock(h.g(), DebugClockPart::Minute, 59);
        const auto sides = free_sides(h, h.x(), h.y());
        if (!sides.empty()) { h.step(sides.front()); h.step(opposite(sides.front())); }
        std::printf("         probe: seeded latch '%c'/'%c', crossed to %d:%02d on Day 3 -> latch now %d/%d, party at %d,%d "
                    "(the device binds no SkyRefresh: see the audit's Batch 53B side finding)\n",
                    char(g_owners->moon_phases[(d0 - 1) * 2]), char(g_owners->moon_phases[(d0 - 1) * 2 + 1]),
                    int(h.g().time.hour), int(h.g().time.minute), int(t.felucca_phase), int(t.trammel_phase), h.x(), h.y());
    }
    // T3: Vas Rel Por '2' from the destination returns to 96,102 at once.
    {
        Harness h;
        const auto a = arrive(h, false);
        const auto origin = init_stone(1);
        auto &q = h.g().spell_quantities;
        for (auto &v : q) v = 0;
        q[46] = 1;
        h.set_mark();
        h.key('c');
        h.key('\r');
        const bool asked = h.mode() == UiMode::TargetSelection && h.rt->ui()->request() == UiRequestId::GatePhase;
        h.key('2');
        if (!expect(a.ok && asked && h.at(origin.x, origin.y) && !h.saw("Failed!") && h.g().spell_quantities[46] == 0, "T3",
                    "Vas Rel Por '2' from the destination returns the party to 96,102 (spell path unchanged)"))
            h.dump("T3");
    }
    // T4: the midnight edge: 00:00-00:09 the gate fires but does not transit
    // (0x494d). A genuine "nothing happens" -- and not the hardware case (21:00).
    {
        Harness h;
        const auto origin = init_stone(1);
        dev_teleport(h, origin.x, origin.y + 1);
        debug_set_clock(h.g(), DebugClockPart::Hour, 0);
        debug_set_clock(h.g(), DebugClockPart::Minute, 5);
        const int before = h.spy.fired;
        const bool active_elsewhere = h.phase() != 1;
        h.step(Direction::North);
        expect(active_elsewhere && h.spy.fired == before + 1 && h.at(origin.x, origin.y), "T4",
               "control: at 00:05 stepping onto a gate fires it but the party stays on it (midnight edge, 0x494d)");
    }
    // T5: and the first transit itself is unchanged by all of the above.
    {
        Harness h;
        const auto a = arrive(h, false);
        expect(a.ok && a.phase == table_phase(h.g().time.day, 21), "T5",
               "the first transit lands on the stone the data's Trammel byte names for this day");
    }
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: batch53b_moongate_return_test <openu5-alpha1-resources.bin>\n");
        return 2;
    }
    openu5_host_virtual_clock_us() = 1'000'000;
    tdeck::AlphaResourcePack pack;
    if (pack.open(argv[1], g_report) != ESP_OK) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    static tdeck::AlphaResourceOwners owners{};
    if (pack.load(owners, g_report) != ESP_OK) { std::fprintf(stderr, "cannot load the resource pack\n"); return 2; }
    g_owners = &owners;
    for (size_t i = 0; i < owners.combat_map_count; ++i) g_arenas.push_back({owners.combat_map_views[i], owners.combat_sprites + i * 16});

    test_reproduction();
    test_reentry_variants();
    test_return_trip();

    std::printf("\nbatch53b_moongate_return: %d/%d checks GREEN, %d RED\n", g_checks - g_failures, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
