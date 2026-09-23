// Batch 29 -- the device's RestServices, against the shipped binaries.
//
// The bed hole-up is CMDS.OVL:0x0552. After the hour prompt it loops in
// 10-minute ticks until g_hour reaches the target (0x063b):
//
//   0647  push 0xa / call 0xffff8ffc   advance_clock(10), ULTIMA.EXE 0x4F7C
//   064e  hour changed to 20 or 5 -> 0xffffbb1a (day/night tiles; H-157)
//   0677  call 0xffffbb0e              stub 0x7a8e -> TOWN.OVL:0x1694, EVERY tick
//   067a  push [0x5896] / [0x5897] / [0x5895]   party x, party y, g_floor
//   0688  call 0x770e                  ULTIMA.EXE 0x368E find_object_at_xy
//   068b  or ax,ax / je 0x634          nothing there -> next tick
//   068f  mov si,-1                    something there -> leave the loop,
//   069d  "Thrown out of bed!\n"       DS 0x422a
//   06a4-06e1  common epilogue: every 'S' -> 'G', inc [g_party_x], redraw.
//
// Stub 0x7a8e has one caller in every module, CMDS 0x0677 (callers_banda.py).
// 0x1694 is ONE routine over the 32-slot actor table. Its NPC half (0x16c9-0x171b ->
// 0x1726 -> 0x1841-0x1856) moves EVERY slot with a non-zero type byte (DS
// 0x659E) to the position its LIVE schedule (DS 0x5D5E) gives for g_hour, on
// every floor, state 1, servedSlot = period, pathIdx -1; the stuck counter is
// not written. It never reads the dead bitmap (DS 0x5B56): a dead NPC is
// absent because the map loader (TOWN 0x123c-0x124e: is_npc_dead 0x0000 ->
// despawn 0x00B0) and the kill path (0x09BC: 0x0052 then 0x00B0) zero its
// type byte. The object half is H-148's hydrate_interior_objects, which the
// core already runs from the same hook.
//
// 0x368E scans object-register slots 1..31 (slot 0 is the party itself),
// comparing +2/+3/+4 with (x, y, floor) -- the floor test is skipped only
// when g_location > 0x7f -- and returns the matched slot's +0 byte. After
// 0x1694 that register holds the current floor's NPCs AND its objects, so
// both count; the flags byte +5 is not read.
//
// Before Batch 29 the device bound RestServices::snap_npcs to an empty
// lambda (H-154) and RestServices::occupied to a constant false (H-155).
// The core consumed both correctly; the device simply fed it nothing.
//
// Expected NPC positions come from this file's own transcription of
// NPC.OVL:0x12E0 over the pack's .NPC tables (the same transcription
// re/tools/npc_schedule_oracle.py uses), never from the port's
// schedule_index.
#include "../main/alpha_runtime.h"

#include "openu5/combat.h"
#include "openu5/rest.h"
#include "openu5/world.h"
#include "openu5/world_commands.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

using namespace openu5;

namespace {

int g_failures = 0, g_checks = 0;
bool expect(bool ok, const char *id, const char *what) {
    ++g_checks;
    std::printf("  %-5s %-6s %s\n", ok ? "GREEN" : "RED", id, what);
    if (!ok) ++g_failures;
    return ok;
}

constexpr uint8_t kCastle = 17;
struct Cell { int x, y; };
struct Place { int x, y, z; };
// Lord British's Castle, ground floor. Both beds are LeftBed 0xAB with
// RightBed 0xAC to the east (the 0x06d5 step lands on it).
constexpr Cell kQuietBed = {9, 7};    // slot 13's bed; slot 13 is elsewhere 11:00-22:59
constexpr Cell kOwnedBed = {12, 10};  // slot 14's bed from 23:00 (times 23,7,15,17)
constexpr uint8_t kGuard1 = 1, kGuard2 = 2, kGuard5 = 5, kOwner = 14, kSlain = 29;
constexpr int kLeftBed = 0xAB, kRightBed = 0xAC;

// NPC.OVL:0x12E0, transcribed independently of the port.
int period(const uint8_t (&t)[4], int hour) {
    int d[4];
    for (int i = 0; i < 4; ++i) d[i] = (hour - t[i]) & 0xff;
    int best = d[0], idx = 0;
    if (best > d[1]) { best = d[1]; idx = 1; }
    if (best > d[2]) { best = d[2]; idx = 2; }
    if (best > d[3]) idx = 1;
    return idx;
}
int16_t z_of(uint8_t z) { return z == 255 ? int16_t(-1) : int16_t(z); }

const tdeck::AlphaResourceOwners *g_owners = nullptr;

const NpcSlot *authored(const NpcLocationData *tables, uint8_t slot) {
    const auto &t = tables[kCastle - 1];
    for (size_t i = 0; i < t.count; ++i)
        if (t.slots[i].slot == slot) return &t.slots[i];
    return nullptr;
}

struct Harness {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    int64_t clock_us = 0;
    size_t mark = 0;

    explicit Harness(int hour, const NpcLocationData *tables = nullptr, uint32_t castle_dead = 0) {
        tdeck::AlphaRuntime::HostTestFixture hf;
        hf.world = g_owners->world;
        hf.location_x = g_owners->location_x; hf.location_y = g_owners->location_y;
        hf.location_count = g_owners->location_count;
        hf.npc_locations = tables ? tables : g_owners->npc_locations;
        hf.pack = g_owners;
        rt->attach_host_test_fixture(hf);
        auto &g = rt->game();
        g.party.character_count = g.party.party_size = 2;
        const char *names[] = {"Avatar", "Iolo"};
        for (int i = 0; i < 2; ++i) {
            auto &ch = g.party.characters[i];
            std::snprintf(ch.name, sizeof(ch.name), "%s", names[i]);
            ch.current_hp = 500; ch.max_hp = 900; ch.status = 'G'; ch.party_status = 0;
            ch.character_class = i ? 'B' : 'A'; ch.dexterity = 30; ch.strength = 30;
        }
        g.party.active_character = 255;
        g.time.hour = uint8_t(hour); g.time.minute = 0;
        g.torch_turns = 500; g.torches = 5; g.karma = 50; g.gold = 321;
        g.npc_dead[kCastle - 1] |= castle_dead;
        g.position.map = {0, 0};
        g.position.xy = {g_owners->location_x[kCastle - 1], g_owners->location_y[kCastle - 1]};
    }
    GameState &g() { return rt->game(); }
    CommandContext &ctx() { return rt->command_context_for_test(); }
    const UiSession &ui() const { return *rt->ui(); }
    bool raw_key(uint8_t code) {
        tdeck::RawInputEvent raw{};
        raw.kind = tdeck::RawInputKind::Keyboard;
        raw.code = code;
        raw.transition = tdeck::KeyTransition::Pressed;
        raw.timestamp_us = (clock_us += 100000);
        return rt->handle(raw);
    }
    bool ball(tdeck::RawInputKind kind) {
        tdeck::RawInputEvent raw{};
        raw.kind = kind;
        raw.timestamp_us = (clock_us += 100000);
        return rt->handle(raw);
    }
    ActionResult run(CommandKind k, Direction d) {
        Command c;
        c.kind = k; c.direction = d; c.has_direction = true;
        return execute_command(ctx(), c);
    }
    void stand(int x, int y) { g().position.xy = {uint8_t(x), uint8_t(y)}; }
    bool enter_castle() {
        raw_key('e');
        return g().position.map.location == kCastle && g().position.map.floor == 0;
    }
    int drawn(Cell c) {
        return ctx().terrain->effective(g_owners->world, MapId{kCastle, 0}, c.x, c.y);
    }
    // The device's own route: (H)ole up, one digit, Enter.
    void hole_up(int hours) {
        set_mark();
        raw_key('h');
        raw_key(uint8_t('0' + hours));
        raw_key('\r');
    }

    NpcActor *npc(uint8_t slot) {
        auto &a = rt->actors();
        for (size_t i = 0; i < a.count; ++i)
            if (a.actors[i].location == kCastle && a.actors[i].schedule.slot == slot) return &a.actors[i];
        return nullptr;
    }
    // Where 0x1694 puts `slot` at `hour`, read from `tables` (the .NPC file).
    Place scheduled(const NpcLocationData *tables, uint8_t slot, int hour) {
        const auto *s = authored(tables ? tables : g_owners->npc_locations, slot);
        if (!s) return {-99, -99, -99};
        const int p = period(s->times, hour);
        return {s->x[p], s->y[p], z_of(s->z[p])};
    }
    bool snapped_to(uint8_t slot, const Place &p, int per) {
        auto *n = npc(slot);
        return n && n->x == p.x && n->y == p.y && n->z == p.z && n->state == 1 && n->served_slot == per &&
               n->path_index == -1;
    }
    // A position a walk could have produced, and a mid-walk machine.
    void displace(uint8_t slot, int x, int y, int z) {
        auto *n = npc(slot);
        if (!n) return;
        n->x = int16_t(x); n->y = int16_t(y); n->z = int16_t(z);
        n->state = 2; n->served_slot = 0; n->path_index = 2; n->stuck = 7;
    }
    void describe(uint8_t slot) {
        auto *n = npc(slot);
        if (!n) { std::printf("         slot %u: absent\n", unsigned(slot)); return; }
        std::printf("         slot %u: (%d,%d,%d) state=%u served=%u path=%d stuck=%d\n", unsigned(slot), n->x, n->y,
                    n->z, unsigned(n->state), unsigned(n->served_slot), int(n->path_index), int(n->stuck));
    }

    void set_mark() { mark = ui().transcript_size(); }
    bool saw(const char *needle) const {
        for (size_t i = mark; i < ui().transcript_size(); ++i) {
            const auto *b = ui().transcript_at(i);
            if (b && std::strstr(b->text, needle)) return true;
        }
        return false;
    }
    void show() const {
        for (size_t i = mark; i < ui().transcript_size(); ++i)
            if (const auto *b = ui().transcript_at(i)) std::printf("         | %s\n", b->text);
    }
    bool clock(int h, int m) { return g().time.hour == h && g().time.minute == m; }
    void when() { std::printf("         clock %02u:%02u, party (%u,%u,%d)\n", unsigned(g().time.hour),
                              unsigned(g().time.minute), unsigned(g().position.xy.x), unsigned(g().position.xy.y),
                              int(g().position.map.floor)); }
    bool all_awake() {
        for (int i = 0; i < g().party.party_size; ++i)
            if (g().party.characters[i].status != 'G') return false;
        return true;
    }

    // The town-fight path the player takes: (A)ttack an adjacent NPC, then
    // walk every member off the south edge on production's own beat loop.
    ActionResult attack(Cell from, Direction d) { stand(from.x, from.y); return run(CommandKind::Attack, d); }
    void walk_off(double seconds) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
        while (ctx().combat && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            if (ctx().combat && ctx().combat_context) {
                auto *a = current_combat_actor(*ctx().combat_context);
                if (a && a->member != 255) a->position.y = int16_t(kCombatGrid - 1);
            }
            ball(tdeck::RawInputKind::TrackballDown);
        }
    }
};

// ---------------------------------------------------------------------------
// H154-C: the host fixture now binds production's RestServices.
// ---------------------------------------------------------------------------
void test_binding() {
    std::printf("H154-C the fixture reaches production's bind_rest_services()\n");
    Harness h(12);
    const auto *rs = h.rt->command_context().rest_services;
    expect(rs && rs->context == h.rt.get() && rs->snap_npcs && rs->occupied && rs->cell_free && rs->karma_record,
           "C1", "context_.rest_services is the runtime's own owner, all four callbacks bound");
}

// ---------------------------------------------------------------------------
// H-154 + H155-C: a quiet bed. Every tick's snap moves NPCs; nothing throws.
// ---------------------------------------------------------------------------
void test_quiet_bed() {
    std::printf("H154-A/H155-C 2 h in the quiet bed (9,7,0) from 12:00; guards displaced\n");
    Harness h(12);
    expect(h.enter_castle() && h.drawn(kQuietBed) == kLeftBed && h.drawn({kQuietBed.x + 1, kQuietBed.y}) == kRightBed,
           "A0", "precondition: in the castle; (9,7) LeftBed, (10,7) RightBed");
    h.displace(kGuard1, 20, 20, 0);     // same floor as the party
    h.displace(kGuard5, 20, 20, 1);     // another floor: 0x1694 moves every floor
    h.stand(kQuietBed.x, kQuietBed.y);
    h.hole_up(2);
    if (!expect(h.saw("Zzzzzzz...") && h.clock(14, 0), "A1", "slept the full two hours: 12 ticks to 14:00"))
        { h.when(); h.show(); }
    const auto *s1 = authored(g_owners->npc_locations, kGuard1);
    const auto *s5 = authored(g_owners->npc_locations, kGuard5);
    const Place g1 = h.scheduled(nullptr, kGuard1, 14), g5 = h.scheduled(nullptr, kGuard5, 14);
    if (!expect(s1 && h.snapped_to(kGuard1, g1, period(s1->times, 14)), "A2",
                "** slot 1 is at its 14:00 schedule cell: state 1, served = period, path -1 **"))
        h.describe(kGuard1);
    if (!expect(s5 && h.snapped_to(kGuard5, g5, period(s5->times, 14)), "A3",
                "** slot 5, displaced to floor 1, is snapped too: 0x1694 ignores the party's floor **"))
        h.describe(kGuard5);
    auto *n1 = h.npc(kGuard1);
    expect(n1 && n1->stuck == 7, "A4", "the stuck counter (DS 0x65C2) is untouched: a reposition, not a rebuild");
    // H155-C: nobody came to (9,7) -- the rest completes and the epilogue runs.
    expect(!h.saw("Thrown out of bed!"), "C2", "H155-C an unoccupied bed never throws");
    if (!expect(h.g().position.xy.x == kQuietBed.x + 1 && h.g().position.xy.y == kQuietBed.y && h.all_awake(),
                "C3", "epilogue 0x06a4-0x06d5: everyone 'G', one step east onto the RightBed"))
        h.when();
}

// ---------------------------------------------------------------------------
// H154-B: the snap never resurrects. Two ways of being gone in 1988.
// ---------------------------------------------------------------------------
void test_dead_stay_dead() {
    std::printf("H154-B1 slot 29 carries a dead bit (DS 0x5B56): absent at entry, absent after 2 h\n");
    {
        Harness h(12, nullptr, uint32_t(1) << kSlain);
        h.enter_castle();
        expect(!h.npc(kSlain) && h.npc(kGuard1), "B1a", "precondition: the loader dropped slot 29, slot 1 is here");
        h.stand(kQuietBed.x, kQuietBed.y);
        h.hole_up(2);
        expect(h.clock(14, 0), "B1b", "the rest ran two hours");
        if (!expect(!h.npc(kSlain), "B1c", "** slot 29 is NOT resurrected by twelve per-tick snaps **"))
            h.describe(kSlain);
    }
    std::printf("H154-B2 guard slot 2 cleared by 0x00B0 in a fight (no dead bit: family 0x70)\n");
    {
        Harness h(12);
        h.enter_castle();
        const bool there = h.npc(kGuard2) != nullptr;
        const auto r = h.attack({13, 29}, Direction::North);      // slot 2 at (13,28) at noon
        expect(there && r.status == CommandStatus::Success && h.ctx().combat, "B2a",
               "(A)ttack guard slot 2 at (13,28): the arena is up");
        h.walk_off(15.0);
        const bool gone = !h.ctx().combat && !h.npc(kGuard2);
        expect(gone && !(h.g().npc_dead[kCastle - 1] & (uint32_t(1) << kGuard2)), "B2b",
               "out of the arena; slot 2 absent and carries NO dead bit");
        // The attack raised the alarm (TOWN 0x958 -> 0x85e): slot 1's LIVE
        // schedule now has times 0 and AI 7, so 0x1694 puts it at x[0],y[0],z[0].
        auto *g1 = h.npc(kGuard1);
        const bool alarmed = g1 && g1->schedule.times[0] == 0 && g1->schedule.times[1] == 0;
        h.displace(kGuard1, 20, 20, 0);
        const int start = h.g().time.hour, end = (start + 2) % 24;
        h.stand(kQuietBed.x, kQuietBed.y);
        h.hole_up(2);
        expect(h.g().time.hour == end, "B2c", "the rest ran two hours");
        if (!expect(!h.npc(kGuard2), "B2d", "** slot 2 is NOT resurrected: 0x1694 skips type-0 slots **"))
            h.describe(kGuard2);
        const auto *s1 = authored(g_owners->npc_locations, kGuard1);
        const Place live = {s1->x[0], s1->y[0], z_of(s1->z[0])};
        if (!expect(alarmed && h.snapped_to(kGuard1, live, 0), "B2e",
                    "** slot 1 snapped from its LIVE (alarmed) schedule, not the .NPC file's **"))
            h.describe(kGuard1);
    }
}

// ---------------------------------------------------------------------------
// H-155: the 0x0688 occupancy probe, NPC and object occupants.
// ---------------------------------------------------------------------------
void test_owner_comes_to_bed() {
    std::printf("H155-A 2 h in slot 14's bed (12,10,0) from 22:00; it is slot 14's from 23:00\n");
    Harness h(22);
    h.enter_castle();
    const Place bed = {kOwnedBed.x, kOwnedBed.y, 0};
    const auto *s14 = authored(g_owners->npc_locations, kOwner);
    expect(s14 && h.drawn(kOwnedBed) == kLeftBed && h.drawn({kOwnedBed.x + 1, kOwnedBed.y}) == kRightBed &&
           period(s14->times, 22) != 0 && h.scheduled(nullptr, kOwner, 23).x == bed.x &&
           h.scheduled(nullptr, kOwner, 23).y == bed.y && h.scheduled(nullptr, kOwner, 23).z == 0,
           "A0", "precondition: (12,10) LeftBed; slot 14 is elsewhere at 22:00 and on it at 23:00");
    h.stand(kOwnedBed.x, kOwnedBed.y);
    h.hole_up(2);
    if (!expect(h.saw("Thrown out of bed!"), "A1", "** \"Thrown out of bed!\" (DS 0x422a) **")) { h.when(); h.show(); }
    if (!expect(h.clock(23, 0), "A2", "** the sleep ended on the 23:00 tick: that tick's clock had already run **"))
        h.when();
    if (!expect(h.snapped_to(kOwner, bed, 0), "A3", "slot 14 is in its bed (the snap came first, 0x0677 < 0x0688)"))
        h.describe(kOwner);
    expect(h.g().position.xy.x == kOwnedBed.x + 1 && h.g().position.xy.y == kOwnedBed.y &&
           h.g().position.map.floor == 0 && h.all_awake(), "A4",
           "the common epilogue still ran: everyone 'G', one step east onto the RightBed");
}

void test_object_on_bed() {
    std::printf("H155-B an object slot re-placed on the bed by 0x1694 (no shipped slot does this)\n");
    // No .NPC object slot (type 1/14/27/30/0xB5/0xB6) stands on a LeftBed in
    // any of the 32 locations, so the object half of the probe cannot fire on
    // shipped data. It is still half of 0x368E's table, so arrange it: a copy
    // of the castle table whose first chest gets period 0 = (9,7,0) from
    // 13:00 (times 13,0,0,0: 0x12E0 picks period 1 at 12, period 0 at 13).
    // It cannot simply stand there already -- an object on the bed hides the
    // 0xAB tile from (H)ole up, which then refuses ("Only in bed!").
    // hydrate_interior_objects (the core's object half of the same hook)
    // re-places it on the first tick that reaches 13:00.
    static NpcLocationData tables[32];
    static std::vector<NpcSlot> castle;
    std::copy(g_owners->npc_locations, g_owners->npc_locations + 32, tables);
    castle.assign(tables[kCastle - 1].slots, tables[kCastle - 1].slots + tables[kCastle - 1].count);
    int chest = -1;
    for (auto &s : castle)
        if (s.type == 1 && chest < 0) {
            chest = s.slot;
            s.x[0] = uint8_t(kQuietBed.x); s.y[0] = uint8_t(kQuietBed.y); s.z[0] = 0;
            s.times[0] = 13; s.times[1] = s.times[2] = s.times[3] = 0;
        }
    tables[kCastle - 1].slots = castle.data();
    const uint8_t t13[4] = {13, 0, 0, 0};
    Harness h(12, tables);
    h.enter_castle();
    auto chest_on_bed = [&] {
        for (const auto &o : h.rt->objects_for_test())
            if (o.chest && o.location == kCastle && o.floor == 0 && o.x == kQuietBed.x && o.y == kQuietBed.y) return true;
        return false;
    };
    expect(chest > 0 && period(t13, 12) == 1 && period(t13, 13) == 0 && !chest_on_bed(), "B0",
           "precondition: the arranged chest is off the bed at 12:xx, due on it at 13:00");
    h.g().time.minute = 50;
    h.stand(kQuietBed.x, kQuietBed.y);
    h.hole_up(2);
    if (!expect(h.saw("Thrown out of bed!") && h.clock(13, 0) && chest_on_bed(), "B1",
                "** an object occupant throws too: the 13:00 tick re-seeded the chest onto the bed **"))
        { h.when(); h.show(); }
    expect(h.g().position.xy.x == kQuietBed.x + 1 && h.all_awake(), "B2", "epilogue: awake, one step east");
}

// ---------------------------------------------------------------------------
// H-160 remains reclassified. Batch 32 adds the optional watch; declining it
// still takes the old unwatched path and leaves the Batch 29 bed checks alone.
// ---------------------------------------------------------------------------
void test_camp_posts_no_watch() {
    std::printf("H160-R outdoor (H)ole up on the device: no watch prompt, no guard\n");
    Harness h(12);
    h.hole_up(2);
    const bool offered = h.ui().mode() == UiMode::YesNo &&
                         std::strstr(h.ui().prompt(),"Wilt thou set a watch?");
    h.raw_key('n');
    const bool slept = h.saw("Zzzzzz...") && (h.saw("Party rested!") || h.saw("Ambushed!"));
    if (!expect(slept, "R1", "the device camp ran")) h.show();
    expect(offered && !h.saw("Who will stand guard?"), "R2",
           "declining the newly available watch uses the old unwatched camp");
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: batch29_rest_wiring_test <openu5-alpha1-resources.bin>\n");
        return 2;
    }
    tdeck::AlphaResourcePack pack;
    tdeck::AlphaResourceReport report{};
    if (pack.open(argv[1], report) != ESP_OK) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    static tdeck::AlphaResourceOwners owners{};
    if (pack.load(owners, report) != ESP_OK) { std::fprintf(stderr, "cannot load the resource pack\n"); return 2; }
    g_owners = &owners;

    test_binding();
    test_quiet_bed();
    test_dead_stay_dead();
    test_owner_comes_to_bed();
    test_object_on_bed();
    test_camp_posts_no_watch();

    std::printf("\nbatch29_rest_wiring: %d/%d checks GREEN, %d RED\n", g_checks - g_failures, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
