// Batch 24 -- when the 1988 game rebuilds the local map: floor change,
// save/load and the end of a town fight, against the shipped binaries.
//
// All three paths end in ONE routine, TOWN.OVL:0x0408, the floor loader. It
// zeroes the open-door tracker [0x594f] (0x041d), re-reads the floor's 0x400
// bytes from the location's .DAT into the map buffer DS 0x6608 (0x045c-0x046f;
// the buffer is outside the save window 0x55A6..0x6605, so nothing written to
// it survives anything), refreshes the hour tiles (0x0508 call 0x170) and
// ONLY IF ITS ARGUMENT IS NON-ZERO calls 0x1694 town_populate_npcs
// (0x0517 `cmp [bp+4],0` / 0x051d).
//
//   caller                                         arg  0x1694 (objects+NPC snap)
//   0x052E stair_transition (stairs 0x0835, (K)limb 0x0bd5)   1   yes
//   0x11F0 town_load_map from boot/Journey Onward (0x00f7)    0   no  -- save window restored verbatim
//   0x09BC town_attack_engine_commit, after 0x6150 returns    0   no  -- 0x5F86 backed the object
//                                                                   register up around the fight
//
// The NPC half of 0x1694: for every slot 1..31 whose type byte (DS 0x659E) is
// non-zero -- a slot cleared by 0x00B0 has type 0 and stays gone -- take the
// schedule period NPC.OVL:0x12E0 picks for g_hour and write X/Y/Z
// (schedule +3/+6/+9) into the live record at 0x1841-0x1856, state = 1
// (0x1856/0x16fc), servedSlot = period (0x1705), pathIdx = -1 (0x170c). That
// is a REPOSITION of every NPC of the location, on every floor; the stuck
// counter (DS 0x65C2), dialog and AI bytes are never touched.
//
// Expected positions come from re/tools/npc_schedule_oracle.py 17 12, which
// reads CASTLE.NPC itself and transcribes 0x12E0.
#include "../main/alpha_runtime.h"

#include "openu5/combat.h"
#include "openu5/world.h"
#include "openu5/world_commands.h"
#include "openu5/world_terrain.h"

#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

using namespace openu5;

namespace {

int g_failures = 0, g_checks = 0;
bool expect(bool ok, const char *id, const char *what) {
    ++g_checks;
    std::printf("  %-5s %-5s %s\n", ok ? "GREEN" : "RED", id, what);
    if (!ok) ++g_failures;
    return ok;
}

constexpr uint8_t kCastle = 17;
constexpr int16_t kBasement = -1;
constexpr int kNoon = 12;
struct Cell { int x, y; };
struct Place { int x, y, z, period; };
// npc_schedule_oracle.py 17 12 (verbatim rows used here).
constexpr uint8_t kGuard1 = 1, kGuard2 = 2, kPrisoner = 21, kSleeper = 26, kUpstairs = 30;
constexpr Place kGuard1At = {17, 28, 0, 1};     // type 0x70, ai 0
constexpr Place kGuard2At = {13, 28, 0, 1};     // type 0x70, ai 0
constexpr Place kSleeperAt = {13, 19, -1, 0};   // type 0x50, ai 0
constexpr Place kUpstairsAt = {19, 18, 1, 2};   // type 0x54, ai 0
constexpr Cell kChests[3] = {{16, 21}, {17, 22}, {13, 23}};
constexpr Cell kVaultDoor = {15, 24}, kDoor = {20, 16};
constexpr int kMagicLockDoor = 0x97, kRegularDoor = 0xB8, kOpenFloor = 0x44;

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

struct Harness {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    int64_t clock_us = 0;

    Harness() {
        tdeck::AlphaRuntime::HostTestFixture hf;
        hf.world = g_owners->world;
        hf.location_x = g_owners->location_x; hf.location_y = g_owners->location_y;
        hf.location_count = g_owners->location_count;
        hf.npc_locations = g_owners->npc_locations;
        hf.pack = g_owners;
        rt->attach_host_test_fixture(hf);
        auto &g = rt->game();
        g.party.character_count = g.party.party_size = 1;
        auto &ch = g.party.characters[0];
        std::snprintf(ch.name, sizeof(ch.name), "Avatar");
        ch.current_hp = ch.max_hp = 900; ch.status = 'G'; ch.party_status = 0; ch.character_class = 'A';
        ch.dexterity = 30; ch.strength = 30;
        g.party.active_character = 255;
        g.time.hour = kNoon; g.time.minute = 0;
        g.torch_turns = 500; g.torches = 5; g.skull_keys = 5; g.karma = 50; g.gold = 321;
        g.position.map = {0, 0};
        g.position.xy = {g_owners->location_x[kCastle - 1], g_owners->location_y[kCastle - 1]};
    }
    GameState &g() { return rt->game(); }
    CommandContext &ctx() { return rt->command_context_for_test(); }
    const std::vector<QuestObject> &pool() const { return rt->objects_for_test(); }
    bool raw_key(uint8_t code, bool alt = false) {
        tdeck::RawInputEvent raw{};
        raw.kind = tdeck::RawInputKind::Keyboard;
        raw.code = code;
        raw.modifiers.alt = alt;
        raw.transition = tdeck::KeyTransition::Pressed;
        raw.timestamp_us = (clock_us += 100000);
        return rt->handle(raw);
    }
    bool key(uint8_t code) { return raw_key(code); }
    bool ball(tdeck::RawInputKind kind) {
        tdeck::RawInputEvent raw{};
        raw.kind = kind;
        raw.timestamp_us = (clock_us += 100000);
        return rt->handle(raw);
    }
    ActionResult run(CommandKind k, Direction d, int16_t item = -1) {
        Command c;
        c.kind = k; c.direction = d; c.has_direction = true; c.item = item;
        return execute_command(ctx(), c);
    }
    void stand(int x, int y) { g().position.xy = {uint8_t(x), uint8_t(y)}; }
    bool at(int16_t floor) const {
        return rt->game().position.map.location == kCastle && rt->game().position.map.floor == floor;
    }
    bool enter_castle() { key('e'); return at(0); }
    bool klimb_down() { stand(1, 1); key('k'); return at(kBasement); }  // authored ladder pair (1,1)
    bool stairs_up() {                                                   // basement stairs 0xC5 at (12,7)
        stand(11, 7);
        run(CommandKind::Move, Direction::East);
        return at(0);
    }

    NpcActor *npc(uint8_t slot) {
        auto &a = rt->actors();
        for (size_t i = 0; i < a.count; ++i)
            if (a.actors[i].location == kCastle && a.actors[i].schedule.slot == slot) return &a.actors[i];
        return nullptr;
    }
    bool is_at(uint8_t slot, const Place &p) {
        auto *n = npc(slot);
        return n && n->x == p.x && n->y == p.y && n->z == p.z;
    }
    bool snapped(uint8_t slot, const Place &p) {
        auto *n = npc(slot);
        return n && n->x == p.x && n->y == p.y && n->z == p.z && n->state == 1 &&
               n->served_slot == p.period && n->path_index == -1;
    }
    // A position a walk could have produced, and a mid-walk machine.
    void displace(uint8_t slot, int x, int y, int z) {
        auto *n = npc(slot);
        if (!n) return;
        n->x = int16_t(x); n->y = int16_t(y); n->z = int16_t(z);
        n->state = 2; n->served_slot = 0; n->path_index = 3; n->stuck = 7;
    }
    void describe(uint8_t slot) {
        auto *n = npc(slot);
        if (!n) { std::printf("         slot %u: absent\n", unsigned(slot)); return; }
        std::printf("         slot %u: (%d,%d,%d) state=%u served=%u path=%d stuck=%d\n", unsigned(slot), n->x, n->y,
                    n->z, unsigned(n->state), unsigned(n->served_slot), int(n->path_index), int(n->stuck));
    }

    const QuestObject *chest(const Cell &c) const {
        for (const auto &o : pool())
            if (o.chest && o.location == kCastle && o.floor == kBasement && o.x == c.x && o.y == c.y) return &o;
        return nullptr;
    }
    size_t loot_at(const Cell &c) const {
        size_t n = 0;
        for (const auto &o : pool())
            if (o.loot && o.location == kCastle && o.floor == kBasement && o.x == c.x && o.y == c.y) ++n;
        return n;
    }
    // Chest (16,21) is north of (16,22).
    void open_first_chest() { stand(16, 22); run(CommandKind::Open, Direction::North); }

    int drawn(const Cell &c) {
        const MapId m{kCastle, kBasement};
        const int t = ctx().terrain->effective(g_owners->world, m, c.x, c.y);
        return open_door_tile(ctx(), m, c.x, c.y, t);
    }
    bool unlock_vault() {  // skull key from the corridor south of (15,24): 0x97 -> 0xB8
        stand(kVaultDoor.x, kVaultDoor.y + 1);
        run(CommandKind::UseItem, Direction::North, 17);
        return drawn(kVaultDoor) == kRegularDoor;
    }
    bool open_door() {     // ordinary door (20,16) from (20,17)
        stand(kDoor.x, kDoor.y + 1);
        run(CommandKind::Open, Direction::North);
        return drawn(kDoor) == kOpenFloor && ctx().commands.door.turns > 0;
    }

    // Device save/load routes.
    void alt_save() { raw_key('s', true); }
    void alt_load() { raw_key('l', true); }
    void menu_save() {
        raw_key('m', true);
        ball(tdeck::RawInputKind::TrackballDown);    // Resume -> Save
        key('\r');
    }
    void menu_load() {
        ball(tdeck::RawInputKind::TrackballDown);    // Save -> Load / Save Management
        key('\r');
        key('\r');                                   // Continue Latest
    }

    // The town-fight path the player takes: (A)ttack an adjacent NPC.
    ActionResult attack(Cell from, Direction d) { stand(from.x, from.y); return run(CommandKind::Attack, d); }
    // Leave the arena the reference's way: walk every member off the south
    // edge. Only the standing-on-the-edge part is staged; enemy beats run on
    // production's own wall-clock service loop.
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
// N: H-161 -- floor change repositions every NPC to its schedule (0x0408(1) -> 0x1694).
// ---------------------------------------------------------------------------
void test_floor_change_snap(bool stairs) {
    const char *id[] = {"N1a", "N1b", "N1c", "N1d", "N1e", "N1f", "N1g", "N1h"};
    const char *sid[] = {"N2a", "N2b", "N2c", "N2d", "N2e", "N2f", "N2g", "N2h"};
    const char **k = stairs ? sid : id;
    std::printf("%s floor change by %s repositions NPCs from their noon schedule\n", stairs ? "N2" : "N1",
                stairs ? "stairs (12,7) up" : "ladder (1,1) down");
    Harness h;
    h.enter_castle();
    if (stairs) h.klimb_down();
    // Three NPCs, one on each side of the step and one on a third floor,
    // stand somewhere their walk took them.
    h.displace(kGuard1, 17, 25, 0);
    h.displace(kSleeper, 14, 17, -1);
    h.displace(kUpstairs, 18, 16, 1);
    const size_t before = h.rt->actors().count;
    expect(h.npc(kGuard1) && h.npc(kSleeper) && h.npc(kUpstairs) && !h.is_at(kGuard1, kGuard1At) &&
               !h.is_at(kSleeper, kSleeperAt) && !h.is_at(kUpstairs, kUpstairsAt),
           k[0], "precondition: slots 1, 26 and 30 are off their schedule cells");
    const bool moved = stairs ? h.stairs_up() : h.klimb_down();
    expect(moved, k[1], stairs ? "real Move onto 0xC5 heading East: \"Up!\" to floor 0"
                               : "real (K)limb on the 0xC9 ladder: floor 0 -> -1");
    const bool g1 = h.snapped(kGuard1, kGuard1At), sl = h.snapped(kSleeper, kSleeperAt),
               up = h.snapped(kUpstairs, kUpstairsAt);
    if (!expect(stairs ? g1 : sl, k[2], "** the landing floor's NPC is at its schedule cell, state 1, period, path -1 **"))
        h.describe(stairs ? kGuard1 : kSleeper);
    if (!expect(stairs ? sl : g1, k[3], "** so is the NPC on the floor just left **")) h.describe(stairs ? kSleeper : kGuard1);
    if (!expect(up, k[4], "** and the NPC on a third floor: 0x1694 walks every slot of the location **"))
        h.describe(kUpstairs);
    // Every NPC not standing on the landing floor had no turn to move since.
    int off = 0, total = 0;
    for (size_t i = 0; i < h.rt->actors().count; ++i) {
        const auto &n = h.rt->actors().actors[i];
        if (n.location != kCastle || n.z == h.g().position.map.floor) continue;
        const int p = period(n.schedule.times, h.g().time.hour);
        ++total;
        if (n.x != n.schedule.x[p] || n.y != n.schedule.y[p] || n.z != z_of(n.schedule.z[p])) ++off;
    }
    char what[128];
    std::snprintf(what, sizeof(what), "every NPC off the landing floor (%d) stands where 0x12E0 puts it", total);
    expect(total > 0 && off == 0, k[5], what);
    expect(h.rt->actors().count == before, k[6], "a reposition: no NPC added, none removed");
    auto *g = h.npc(kGuard1);
    expect(g && g->stuck == 7, k[7], "the stuck counter (DS 0x65C2) is left alone -- not a rebuild");
}

void test_pass_does_not_snap() {
    std::printf("N3 an ordinary turn does not reposition (0x1694 is reached from 0x0408(1) and the bed only)\n");
    Harness h;
    h.enter_castle();
    auto *n = h.npc(kUpstairs);
    if (n) { n->x = 18; n->y = 16; }  // keep its period: nothing for the scheduler to do at noon
    h.run(CommandKind::Pass, Direction::North);
    h.run(CommandKind::Pass, Direction::North);
    expect(h.is_at(kUpstairs, {18, 16, 1, 2}), "N3a", "two (P)asses on floor 0 leave slot 30 where it was");
}

// ---------------------------------------------------------------------------
// S: H-162 -- load is 0x11F0(fresh=0) -> 0x0408(0): the door closes, nothing else resets.
// ---------------------------------------------------------------------------
void test_save_load(bool menu) {
    const char *ids[] = {"S1a", "S1b", "S1c", "S1d", "S1e", "S1f", "S1g", "S1h", "S1i"};
    const char *mids[] = {"S2a", "S2b", "S2c", "S2d", "S2e", "S2f", "S2g", "S2h", "S2i"};
    const char **k = menu ? mids : ids;
    std::printf("%s open door across %s\n", menu ? "S2" : "S1",
                menu ? "System Menu Save / Continue Latest (synchronize_loaded_world)" : "Alt+S / Alt+L");
    Harness h;
    h.enter_castle();
    h.klimb_down();
    // Genuinely persistent state, all in the 1988 save window or the .GAM:
    h.open_first_chest();                       // object register DS 0x5C5A
    h.displace(kGuard1, 17, 25, 0);             // live NPC table DS 0x5F5E
    const auto *c0 = h.chest(kChests[0]);
    const size_t piles = h.loot_at(kChests[0]);
    expect(!c0 && piles > 0 && h.chest(kChests[1]) && h.chest(kChests[2]), k[0],
           "precondition: chest (16,21) opened, its loot on the floor, the other two closed");
    // Transient local-map state:
    expect(h.unlock_vault(), k[1], "skull key on (15,24): 0x97 -> 0xB8 (transient terrain)");
    expect(h.drawn(kDoor) == kRegularDoor && h.open_door(), k[2], "(O)pen the ordinary door (20,16): drawn open");
    const Cell here{h.g().position.xy.x, h.g().position.xy.y};
    const auto keys = h.g().skull_keys;
    const auto karma = h.g().karma;             // the chest cost town karma: whatever it is now is saved
    if (menu) h.menu_save(); else h.alt_save();
    h.g().gold = 999;                           // after the save: a load must bring 321 back
    if (menu) h.menu_load(); else h.alt_load();
    expect(h.g().gold == 321 && h.at(kBasement) && h.g().position.xy.x == here.x && h.g().position.xy.y == here.y,
           k[3], "the load really happened: gold 999 -> 321, same floor and cell");
    const int t = h.drawn(kDoor);
    if (!expect(t == kRegularDoor && h.ctx().commands.door.turns == 0, k[4],
                "** 0x0408(0): the open door is closed again (tracker [0x594f] = 0) **"))
        std::printf("         door (20,16) drawn 0x%02x, tracker turns=%d\n", unsigned(t), h.ctx().commands.door.turns);
    expect(h.drawn(kVaultDoor) == kMagicLockDoor, k[5], "the skull-keyed lock is 0x97 again (map buffer not saved)");
    expect(!h.chest(kChests[0]) && h.loot_at(kChests[0]) == piles && h.chest(kChests[1]) && h.chest(kChests[2]), k[6],
           "the opened chest stays opened with its loot; no refill (fresh = 0 skips 0x1694)");
    if (!expect(h.is_at(kGuard1, {17, 25, 0, 0}), k[7], "slot 1 is still where it walked: the NPC band loads verbatim"))
        h.describe(kGuard1);
    expect(h.g().skull_keys == keys && keys == 4 && h.g().karma == karma, k[8],
           "skull keys (one used) and karma as saved");
}

// ---------------------------------------------------------------------------
// F: H-163 -- after a town fight, 0x09BC re-reads the floor with 0x0408(0).
// ---------------------------------------------------------------------------
void test_town_fight() {
    std::printf("F1 a basement fight (Attack slot 21 at (12,10)) ends in 0x0408(0)\n");
    Harness h;
    h.enter_castle();
    h.klimb_down();
    h.open_first_chest();
    h.displace(kGuard1, 17, 25, 0);
    expect(h.unlock_vault() && h.open_door(), "F1a", "precondition: vault unlocked to 0xB8, door (20,16) open");
    // Slot 21 (ai 7) may have stepped inside its cell; attack it from the south wherever it stands.
    auto *p = h.npc(kPrisoner);
    const auto r = p ? h.attack({p->x, p->y + 1}, Direction::North) : ActionResult{};
    expect(r.status == CommandStatus::Success && h.ctx().combat, "F1b", "(A)ttack slot 21: the arena is up");
    h.walk_off(10.0);
    expect(!h.ctx().combat && !h.npc(kPrisoner), "F1c",
           "the party walked off; slot 21 is gone either way (0x09BC is unconditional)");
    const int v = h.drawn(kVaultDoor);
    if (!expect(v == kMagicLockDoor, "F1d", "** the floor re-read restores the authored 0x97 lock **"))
        std::printf("         vault door drawn 0x%02x\n", unsigned(v));
    expect(h.drawn(kDoor) == kRegularDoor && h.ctx().commands.door.turns == 0, "F1e",
           "the open door is closed (COMBAT 0x0bcf already zeroed [0x594f] on entry)");
    expect(!h.chest(kChests[0]) && h.chest(kChests[1]) && h.chest(kChests[2]), "F1f",
           "no refill and no wipe: 0x0408(0) skips 0x1694 and 0x5F86 restored the object register");
    if (!expect(h.is_at(kGuard1, {17, 25, 0, 0}), "F1g", "no NPC reposition either (argument 0)")) h.describe(kGuard1);
}

void test_fight_then_floor_change() {
    std::printf("F2 a guard killed-or-fled from in a fight stays gone across the next floor change\n");
    Harness h;
    h.enter_castle();
    const bool there = h.is_at(kGuard2, kGuard2At);
    const auto r = h.attack({13, 29}, Direction::North);   // slot 2 at (13,28)
    expect(there && r.status == CommandStatus::Success && h.ctx().combat, "F2a",
           "(A)ttack the guard slot 2 at (13,28): the arena is up");
    h.walk_off(15.0);
    expect(!h.ctx().combat && !h.npc(kGuard2), "F2b", "out of the arena; slot 2 cleared by 0x00B0 (no dead bit: family 0x70)");
    // Attacking a person-family actor raised the alarm (TOWN 0x958): for each
    // guard 0x85e zeroed the four schedule times in the LIVE table (DS 0x5D6A,
    // 0x0890-0x0896) and set its AI to 7 (0x08c2). 0x1694 reads that table, not
    // the .NPC file, so every period is 0 at any hour: slot 1 goes to x[0],y[0],z[0].
    auto *g1 = h.npc(kGuard1);
    expect(g1 && g1->schedule.times[0] == 0 && g1->schedule.times[1] == 0 && g1->schedule.ai[1] == 7, "F2c",
           "the alarm rewrote slot 1's live schedule: times 0, AI 7 (0x85e)");
    h.displace(kGuard1, 17, 25, 0);
    h.klimb_down();
    constexpr Place kGuard1Alarmed = {17, 7, 0, 0};  // oracle row slot 1, period 0
    if (!expect(h.at(kBasement) && h.snapped(kGuard1, kGuard1Alarmed), "F2d",
                "(K)limb down: slot 1 repositioned from the LIVE schedule, to (17,7,0) -- not the file's (17,28,0)"))
        h.describe(kGuard1);
    expect(!h.npc(kGuard2), "F2e", "** slot 2 is NOT resurrected: 0x1694 skips type-0 slots; a .NPC re-read would not **");
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: batch24_reload_parity_test <openu5-alpha1-resources.bin>\n");
        return 2;
    }
    tdeck::AlphaResourcePack pack;
    tdeck::AlphaResourceReport report{};
    if (pack.open(argv[1], report) != ESP_OK) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    static tdeck::AlphaResourceOwners owners{};
    if (pack.load(owners, report) != ESP_OK) { std::fprintf(stderr, "cannot load the resource pack\n"); return 2; }
    g_owners = &owners;

    test_floor_change_snap(false);
    test_floor_change_snap(true);
    test_pass_does_not_snap();
    test_save_load(false);
    test_save_load(true);
    test_town_fight();
    test_fight_then_floor_change();

    std::printf("\nbatch24_reload_parity: %d/%d checks GREEN, %d RED\n", g_checks - g_failures, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
