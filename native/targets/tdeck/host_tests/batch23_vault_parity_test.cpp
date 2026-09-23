// Batch 23 -- Lord British's basement vault: chest loot generation and the
// chest-reset / door-relock lifecycle, against the 1988 binaries.
//
// Everything asserted here was recovered from the shipped executables, not
// from the TypeScript port (see GAMEPLAY_INTEGRATION_AUDIT.md, Batch 23):
//
//  LOOT
//   TOWN.OVL:0x1726 places a type-1 .NPC slot with object byte +5 = 0x1E
//     (0x1795 `mov word [bp-6],0x1e`, written by kernel 0x3A74).
//   SJOG.OVL:0x112C open_chest_world reads that byte AT (O)PEN TIME and rolls
//     SJOG 0x1040 loot_fixed then 0x10B8 loot_random with it; 0x0F88
//     loot_place derives each quantity. Tables: DATA.OVL DS 0x4124/0x412C/
//     0x4134 (8 fixed rows) and DS 0x413C/0x416C (48 equipment rows).
//   Kernel rand is ULTIMA.EXE:0x2092.
//   The expected vectors below come from re/tools/chest_loot_oracle.py, an
//   independent transcription of those routines that reads the tables out of
//   DATA.OVL itself.
//
//  LIFECYCLE
//   Every stair or ladder step is TOWN.OVL:0x052E stair_transition, which
//   ends in `push 1; call 0x408`. 0x408 re-reads the new floor's 0x400-byte
//   chunk from disk into the map buffer DS 0x6608, zeroes the open-door
//   tracker [0x594f], and -- because its argument is non-zero -- calls
//   0x1694 town_populate_npcs (0x0517 `cmp [bp+4],0` / 0x051d `call 0x1694`),
//   which wipes all 31 object slots and re-places the current floor's .NPC
//   objects: the chests come back with contents 0x1E and the skull-keyed
//   door (map tile 0x97 -> 0xB8) is 0x97 again.
//   Bed hole-up (CMDS.OVL:0x0677) calls 0x1694 directly and never reaches
//   0x408: chests refill, the door stays as the player left it.
//   Neither routine is called by the ordinary clock tick.
#include "../main/alpha_runtime.h"

#include "openu5/loot.h"
#include "openu5/world.h"
#include "openu5/world_commands.h"
#include "openu5/world_terrain.h"

#include <cstdio>
#include <memory>
#include <utility>
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
struct Cell { int x, y; };
constexpr Cell kChests[3] = {{16, 21}, {17, 22}, {13, 23}};
constexpr Cell kVaultDoor = {15, 24};
constexpr int kMagicLockDoor = 0x97, kRegularDoor = 0xB8, kOpenFloor = 0x44;
constexpr uint8_t kContents = 0x1E; // TOWN.OVL:0x1795

using Piece = std::pair<int, int>; // (object id, byte +5)

// re/tools/chest_loot_oracle.py <seed> 30, verbatim.
struct Vector { uint16_t seed; std::vector<Piece> pieces; };
const Vector kVectors[3] = {
    {0x0073, {{15, 1}, {13, 1}, {7, 1}, {4, 1}, {2, 53}, {5, 24}}},   // Mace (equip 24), scroll 1, keys
    {0x00d0, {{13, 2}, {7, 2}, {3, 4}, {5, 29}, {5, 29}, {5, 27}}},   // potion 4, Quarrels x2, Arrows
    {0x0181, {{15, 1}, {8, 1}, {4, 0}, {2, 74}, {1, 4}, {5, 19}}},    // gems, nested chest, Flaming Oil
};
// The same three seeds at contents 8 (the old placeholder), for the record:
//   0x0073 -> (15,1) (13,1) (2,11)   0x00d0 -> (13,2) (2,12)   0x0181 -> (15,1) (2,16) (5,16)

// DATA.OVL, fileoff = DS + 0x10.
constexpr int kFixedItem[8] = {1, 2, 3, 4, 7, 8, 13, 15};
constexpr int kFixedGuard[8] = {25, 3, 17, 17, 9, 15, 7, 7};
constexpr int kFixedMax[8] = {10, 90, 8, 8, 2, 2, 2, 2};
constexpr int kRandItem[48] = {9, 9, 9, 9, 6, 6, 6, 6, 6, 11, 11, 11, 11, 11, 11, 11,
                               5, 5, 5, 5, 5, 5, 5, 5, 5, 5,  5,  5,  5,  5,  5,  5,
                               5, 5, 5, 5, 5, 5, 5, 5, 5, 5,  10, 10, 10, 12, 12, 12};
constexpr int kRandGuard[48] = {10, 10, 15, 20, 10, 15, 20, 28, 255, 15, 15, 20, 20, 20, 24, 255,
                                5,  10, 10, 10, 10, 10, 10, 10, 15,  15, 15, 10, 15, 10, 20, 20,
                                20, 20, 20, 255, 23, 23, 23, 255, 255, 255, 23, 23, 23, 23, 15, 255};

const tdeck::AlphaResourceOwners *g_owners = nullptr;

const MapData *small_map(int16_t floor) {
    const auto &w = g_owners->world;
    for (size_t i = 0; i < w.small_map_count; ++i)
        if (w.small_maps[i].id.location == kCastle && w.small_maps[i].id.floor == floor)
            return &w.small_maps[i];
    return nullptr;
}
int authored(int16_t floor, int x, int y) {
    const auto *m = small_map(floor);
    return m ? m->tiles[y * 32 + x] : -1;
}

struct Harness {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    int64_t clock_us = 0;

    Harness() {
        tdeck::AlphaRuntime::HostTestFixture hf;
        hf.world = g_owners->world;
        hf.location_x = g_owners->location_x; hf.location_y = g_owners->location_y;
        hf.location_count = g_owners->location_count;
        hf.npc_locations = g_owners->npc_locations;
        rt->attach_host_test_fixture(hf);
        auto &g = rt->game();
        g.party.character_count = g.party.party_size = 1;
        auto &ch = g.party.characters[0];
        std::snprintf(ch.name, sizeof(ch.name), "Avatar");
        ch.current_hp = ch.max_hp = 200; ch.status = 'G'; ch.party_status = 0; ch.character_class = 'A';
        ch.dexterity = 30;
        g.party.active_character = 255;
        g.time.hour = 12;
        g.torch_turns = 500; g.torches = 5; g.skull_keys = 5; g.karma = 50;
        g.position.map = {0, 0};
        g.position.xy = {g_owners->location_x[kCastle - 1], g_owners->location_y[kCastle - 1]};
    }
    GameState &g() { return rt->game(); }
    CommandContext &ctx() { return rt->command_context_for_test(); }
    const std::vector<QuestObject> &pool() const { return rt->objects_for_test(); }
    bool key(uint8_t code) {
        tdeck::RawInputEvent raw{};
        raw.kind = tdeck::RawInputKind::Keyboard;
        raw.code = code;
        raw.transition = tdeck::KeyTransition::Pressed;
        raw.timestamp_us = (clock_us += 100000);
        return rt->handle(raw);
    }
    // AlphaRuntime::command() forwards these kinds to execute_command()
    // unchanged; its extra arms are for Search, Enter logging and combat.
    ActionResult run(CommandKind k, Direction d, int16_t item = -1) {
        Command c;
        c.kind = k; c.direction = d; c.has_direction = true; c.item = item;
        return execute_command(ctx(), c);
    }
    void stand(int x, int y) { g().position.xy = {uint8_t(x), uint8_t(y)}; }
    bool at(int16_t floor) const {
        return rt->game().position.map.location == kCastle && rt->game().position.map.floor == floor;
    }

    // Walk-in route (Batch 22): (E)nter from the overworld, (K)limb down the
    // authored floor-0 ladder. Only the ladder cell is placed by hand.
    bool enter_basement() {
        key('e');
        if (!at(0)) return false;
        stand(1, 1); // authored 0xC9 over the basement's 0xC8
        key('k');
        return at(kBasement);
    }
    // (K)limb up the basement ladder / back down it.
    bool klimb_up() { stand(1, 1); key('k'); return at(0); }
    bool klimb_down() { key('k'); return at(kBasement); }
    // The basement's east stairs (12,7). Every leg is a real Move;
    // TOWN.OVL:0x0810 applies 0x052E to the cell entered.
    bool stairs_up() {
        stand(11, 7);
        run(CommandKind::Move, Direction::East);    // onto 0xC5 heading East -> "Up!"
        return at(0);
    }
    bool stairs_down() {
        run(CommandKind::Move, Direction::East);    // off the stairs to (13,7)
        run(CommandKind::Move, Direction::West);    // onto 0xC5 heading West -> "Down!"
        return at(kBasement);
    }
    bool up(bool stairs) { return stairs ? stairs_up() : klimb_up(); }
    bool down(bool stairs) { return stairs ? stairs_down() : klimb_down(); }

    const QuestObject *chest(const Cell &c) const {
        for (const auto &o : pool())
            if (o.chest && o.location == kCastle && o.floor == kBasement && o.x == c.x && o.y == c.y) return &o;
        return nullptr;
    }
    int chests() const { int n = 0; for (auto &c : kChests) n += chest(c) ? 1 : 0; return n; }
    int chest_copies() const {
        int n = 0;
        for (const auto &o : pool()) if (o.chest && o.location == kCastle && o.floor == kBasement) ++n;
        return n;
    }
    std::vector<Piece> loot(const Cell &c) const {
        std::vector<Piece> v;
        for (const auto &o : pool())
            if (o.loot && o.location == kCastle && o.floor == kBasement && o.x == c.x && o.y == c.y)
                v.push_back({o.item_id, o.quality});
        return v;
    }
    size_t loot_piles() const {
        size_t n = 0;
        for (const auto &o : pool()) if (o.loot && o.location == kCastle) ++n;
        return n;
    }
    // Chest (16,21) north of (16,22), chest (17,22) east of it, chest (13,23)
    // west of (14,23). (16,22) is an authored barrel; the vault is barrels
    // around three floor cells, so the player pushes one first. The test only
    // needs a cell adjacent to each chest -- the command itself is real.
    ActionResult open(int index) {
        static constexpr Cell from[3] = {{16, 22}, {16, 22}, {14, 23}};
        static constexpr Direction dir[3] = {Direction::North, Direction::East, Direction::West};
        stand(from[index].x, from[index].y);
        return run(CommandKind::Open, dir[index]);
    }
    ActionResult get(int index) {
        static constexpr Cell from[3] = {{16, 22}, {16, 22}, {14, 23}};
        static constexpr Direction dir[3] = {Direction::North, Direction::East, Direction::West};
        stand(from[index].x, from[index].y);
        return run(CommandKind::Get, dir[index]);
    }
    void loot_all() { for (int i = 0; i < 3; ++i) open(i); }

    // Door as drawn: terrain layers, then the Open-door countdown overlay.
    int door() {
        const MapId m{kCastle, kBasement};
        const int t = ctx().terrain->effective(g_owners->world, m, kVaultDoor.x, kVaultDoor.y);
        return open_door_tile(ctx(), m, kVaultDoor.x, kVaultDoor.y, t);
    }
    // Skull key from the corridor south of the door, then (O)pen it.
    bool unlock_and_open_door() {
        stand(kVaultDoor.x, kVaultDoor.y + 1);
        run(CommandKind::UseItem, Direction::North, 17);
        if (door() != kRegularDoor) return false;
        run(CommandKind::Open, Direction::North);
        return door() == kOpenFloor;
    }
};

// ---------------------------------------------------------------------------
// V: the loot contract.
// ---------------------------------------------------------------------------
struct Recorder {
    OriginalRng rng;
    std::vector<std::pair<int, int>> calls;
    std::vector<int> script;
    size_t cursor = 0;
    Rand rand() {
        return {this, [](void *p, int32_t lo, int32_t hi) {
                    auto &r = *static_cast<Recorder *>(p);
                    r.calls.push_back({lo, hi});
                    if (r.cursor < r.script.size()) return int32_t(r.script[r.cursor++]);
                    return r.rng.next(lo, hi).value;
                }};
    }
};
std::vector<Piece> roll(int contents, Recorder &r) {
    std::vector<Piece> out;
    chest_loot(contents, r.rand(), {&out, [](void *p, LootGrant g) {
                                        static_cast<std::vector<Piece> *>(p)->push_back({g.id, g.quantity});
                                    }});
    return out;
}

void test_loot_mechanism() {
    std::printf("V1 exact RNG call sequence of SJOG 0x1040/0x10B8/0x0F88 at contents 0x1E\n");
    {
        // chest_loot_oracle.py 0x0073 30 -- the call log, in order.
        const std::vector<std::pair<int, int>> calls = {
            {1, 30}, {1, 2}, {1, 30}, {1, 2}, {1, 30}, {1, 30}, {1, 2}, {1, 30}, {1, 8}, {1, 30}, {1, 30},
            {1, 90}, {1, 90}, {1, 30}, {0, 47}, {0, 47}, {0, 47}, {1, 30}, {0, 47}, {1, 30}, {0, 47},
            {1, 30}, {0, 47}, {1, 30}, {0, 47}, {1, 30}, {0, 47}, {0, 47}, {1, 30}, {0, 47}, {1, 30},
            {0, 47}, {1, 30}, {0, 47}, {1, 30}, {0, 47}, {0, 47}, {1, 30}, {0, 47}, {1, 30}, {0, 47}, {1, 30}};
        Recorder r{OriginalRng{0x0073}};
        const auto p = roll(kContents, r);
        expect(r.calls == calls, "V1a", "seed 0x0073: 42 draws with exactly the original (lo,hi) ranges, in order");
        expect(r.rng.get_seed() == 0xe0fc, "V1b", "seed 0x0073: final RNG state 0xe0fc, as the oracle");
        expect(p == kVectors[0].pieces, "V1c", "seed 0x0073: pieces equal the oracle's");
    }
    {
        const uint16_t finals[3] = {0xe0fc, 0x3cb3, 0xa562};
        bool ok = true;
        for (int i = 0; i < 3; ++i) {
            Recorder r{OriginalRng{kVectors[i].seed}};
            ok &= roll(kContents, r) == kVectors[i].pieces && r.rng.get_seed() == finals[i];
        }
        expect(ok, "V1d", "all three oracle vectors: pieces and final RNG state match");
    }

    std::printf("V2 every fixed row is reachable at 0x1E, with the original quantity rules\n");
    {
        // Each row: guard roll 30 (pass), then the quantity roll(s) at their max.
        Recorder r;
        r.script = {30, 2, 30, 2, 30, 2, 30, 2, 30, 8, 30, 8, 30, 90, 90, 30, 10, 30};
        for (int i = 0; i < kContents / 2 + 1; ++i) r.script.push_back(47); // Ankh: guard 255, never rolls
        const auto p = roll(kContents, r);
        const std::vector<Piece> want = {{15, 2}, {13, 2}, {8, 2}, {7, 2}, {4, 7}, {3, 7}, {2, 90}, {1, 30}};
        expect(p == want, "V2a", "food, torches, gems, keys, scroll 7, potion 7, 90 gold, nested chest (contents 30)");
        expect(r.cursor == r.script.size() && r.calls.size() == r.script.size(), "V2b",
               "and the RNG was asked exactly 34 times: no roll for a 255 guard");
    }

    std::printf("V3 the 48-row equipment table, index by index, at 0x1E\n");
    {
        int wrong = 0, reachable = 0;
        for (int idx = 0; idx < 48; ++idx) {
            Recorder r;
            for (int row = 0; row < 8; ++row) r.script.push_back(1);     // every fixed row fails its guard roll
            for (int d = 0; d < kContents / 2 + 1; ++d) {
                r.script.push_back(idx);
                if (kRandGuard[idx] <= kContents) r.script.push_back(30);
            }
            const auto p = roll(kContents, r);
            const bool can = kRandGuard[idx] <= kContents;
            std::vector<Piece> want;
            if (can) want.assign(size_t(kContents / 2 + 1), Piece{kRandItem[idx], idx});
            if (p != want) ++wrong;
            if (can) ++reachable;
        }
        expect(wrong == 0, "V3a", "each index yields (RAND_ITEM[idx], idx) sixteen times, or nothing if guard 255");
        expect(reachable == 41, "V3b", "41 of 48 equipment rows reachable; 8,15,35,39,40,41,47 never");
    }
    (void)kFixedItem; (void)kFixedGuard; (void)kFixedMax;
}

void test_loot_runtime() {
    std::printf("V4 hydrated vault chests carry the 1988 contents byte (TOWN.OVL:0x1795)\n");
    Harness h;
    expect(h.enter_basement(), "V4a", "walk-in: (E)nter, (K)limb down to floor -1");
    bool contents = h.chests() == 3;
    for (auto &c : kChests) if (auto *o = h.chest(c)) contents &= o->contents == kContents && !o->trapped;
    if (!expect(contents, "V4b", "all three chests: contents 0x1E, untrapped")) {
        for (auto &c : kChests) if (auto *o = h.chest(c))
            std::printf("         chest (%d,%d) contents=%ld trapped=%d\n", c.x, c.y, long(o->contents), o->trapped);
    }

    std::printf("V5 real (O)pen through the runtime, one oracle seed per chest\n");
    for (int i = 0; i < 3; ++i) {
        h.g().rng = OriginalRng{kVectors[i].seed};
        const auto r = h.open(i);
        const auto got = h.loot(kChests[i]);
        char what[128];
        std::snprintf(what, sizeof(what), "chest (%d,%d) seed 0x%04x: floor pieces equal the oracle's",
                      kChests[i].x, kChests[i].y, unsigned(kVectors[i].seed));
        const char *ids[3] = {"V5a", "V5b", "V5c"};
        if (!expect(r.status == CommandStatus::Success && !h.chest(kChests[i]) && got == kVectors[i].pieces, ids[i], what)) {
            std::printf("         status=%d chest_left=%d pieces:", int(r.status), h.chest(kChests[i]) != nullptr);
            for (auto &p : got) std::printf(" (%d,%d)", p.first, p.second);
            std::printf("\n");
        }
    }

    std::printf("V6 (G)et carries every piece of chest 1 into the inventory\n");
    {
        const auto before = h.g();
        for (int n = 0; n < 12 && !h.loot(kChests[0]).empty(); ++n) h.get(0);
        const auto &g = h.g();
        expect(h.loot(kChests[0]).empty(), "V6a", "the pile at (16,21) is empty after the (G)ets");
        expect(g.equipment_quantities[24] == before.equipment_quantities[24] + 1, "V6b", "a Mace (equipment 24)");
        expect(g.scroll_quantities[1] == before.scroll_quantities[1] + 1, "V6c", "a scroll of index 1");
        expect(g.keys == before.keys + 1 && g.gold == before.gold + 53 && g.food == before.food + 1 &&
                   g.torches == before.torches + 1, "V6d", "1 key, 53 gold, 1 food, 1 torch");
    }
}

// ---------------------------------------------------------------------------
// L: the chest lifecycle.
// ---------------------------------------------------------------------------
void test_chest_lifecycle(bool stairs) {
    const char *how = stairs ? "stairs (12,7)" : "ladder (1,1)";
    std::printf("L%d chests across a floor round trip by %s\n", stairs ? 2 : 1, how);
    Harness h;
    h.enter_basement();
    expect(h.chests() == 3, stairs ? "L2a" : "L1a", "precondition: three chests on entry");
    h.loot_all();
    expect(h.chests() == 0 && h.loot_piles() > 0, stairs ? "L2b" : "L1b", "all three opened; loot left on the floor");
    const bool moved = h.up(stairs) && h.down(stairs);
    expect(moved, stairs ? "L2c" : "L1c", "up to floor 0 and back down to floor -1");
    bool fresh = h.chests() == 3 && h.chest_copies() == 3;
    for (auto &c : kChests) if (auto *o = h.chest(c)) fresh &= o->contents == kContents;
    expect(fresh, stairs ? "L2d" : "L1d", "** 0x408(1) -> 0x1694: all three chests back, contents 0x1E, no duplicates **");
    expect(h.loot_piles() == 0, stairs ? "L2e" : "L1e", "uncollected floor loot is gone (the object register was wiped)");
    // Loot is rolled at (O)pen from the fresh byte, so it rerolls.
    h.g().rng = OriginalRng{kVectors[1].seed};
    h.open(0);
    expect(h.loot(kChests[0]) == kVectors[1].pieces, stairs ? "L2f" : "L1f",
           "the refilled chest rolls anew: seed 0x00d0 gives the oracle's pieces");
}

void test_chest_negatives() {
    std::printf("L3 an ordinary turn does not refill (0x1694 has two callers; neither is the clock)\n");
    {
        Harness h;
        h.enter_basement();
        h.loot_all();
        for (int i = 0; i < 5; ++i) h.run(CommandKind::Pass, Direction::North);
        expect(h.chests() == 0, "L3a", "five (P)asses in the basement leave the vault empty");
    }
    std::printf("L4 leaving the castle and coming back refills (town_load_town_map(fresh=1))\n");
    {
        Harness h;
        h.enter_basement();
        h.loot_all();
        h.stand(1, 1);
        h.key('k');
        h.stand(15, 31);
        h.run(CommandKind::Move, Direction::South); // off the south edge: "Leave?" prompt
        h.key('y');
        expect(h.g().position.map.location == 0, "L4a", "out on the overworld");
        h.key('e');
        h.stand(1, 1);
        h.key('k');
        expect(h.at(kBasement) && h.chests() == 3 && h.chest_copies() == 3, "L4b", "re-entry: three chests again");
    }
}

// ---------------------------------------------------------------------------
// D: the vault door.
// ---------------------------------------------------------------------------
void test_door_lifecycle(bool stairs) {
    std::printf("D%d vault door across a floor round trip by %s\n", stairs ? 2 : 1, stairs ? "stairs (12,7)" : "ladder (1,1)");
    Harness h;
    h.enter_basement();
    expect(authored(kBasement, kVaultDoor.x, kVaultDoor.y) == kMagicLockDoor && h.door() == kMagicLockDoor,
           stairs ? "D2a" : "D1a", "(15,24) is authored 0x97 MagicLockDoor and drawn as such");
    expect(h.unlock_and_open_door(), stairs ? "D2b" : "D1b", "skull key: 0x97 -> 0xB8; (O)pen: drawn open");
    const bool went_up = h.up(stairs);
    // Checked on arrival upstairs: that leg is one turn, the countdown is four.
    expect(went_up && h.ctx().commands.door.turns == 0, stairs ? "D2d" : "D1d",
           "the open-door tracker is cleared by the first floor change ([0x594f] = 0)");
    expect(h.down(stairs), stairs ? "D2c" : "D1c", "and back down to floor -1");
    const int t = h.door();
    if (!expect(t == kMagicLockDoor, stairs ? "D2e" : "D1e",
                "** the floor re-read restores the authored 0x97: closed AND magically locked **"))
        std::printf("         door tile now 0x%02x\n", unsigned(t));
}

void test_door_negatives() {
    std::printf("D3 bed hole-up refills the chests but leaves the door as it was\n");
    {
        Harness h;
        h.enter_basement();
        h.loot_all();
        // Unlocked but NOT opened: an open door's countdown overlay would hide
        // the tile underneath, and the tile is what a map re-read would change.
        h.stand(kVaultDoor.x, kVaultDoor.y + 1);
        h.run(CommandKind::UseItem, Direction::North, 17);
        h.stand(16, 19); // authored 0xAB bed
        Command rest; rest.kind = CommandKind::Rest; rest.hours = 1;
        execute_command(h.ctx(), rest);
        expect(h.chests() == 3, "D3a", "CMDS 0x0677 -> 0x1694: chests are back (Batch 21B)");
        const int t = h.door();
        if (!expect(t == kRegularDoor, "D3b", "and the door is still the unlocked 0xB8 (0x1694 never touches DS 0x6608)"))
            std::printf("         door tile now 0x%02x\n", unsigned(t));
    }
    std::printf("D4 leaving the castle and coming back relocks\n");
    {
        Harness h;
        h.enter_basement();
        h.unlock_and_open_door();
        h.stand(1, 1);
        h.key('k');
        h.stand(15, 31);
        h.run(CommandKind::Move, Direction::South); // off the south edge: "Leave?" prompt
        h.key('y');
        h.key('e');
        h.stand(1, 1);
        h.key('k');
        expect(h.at(kBasement) && h.door() == kMagicLockDoor, "D4a", "re-entry: the door is 0x97 again");
    }
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: batch23_vault_parity_test <openu5-alpha1-resources.bin>\n");
        return 2;
    }
    tdeck::AlphaResourcePack pack;
    tdeck::AlphaResourceReport report{};
    if (pack.open(argv[1], report) != ESP_OK) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    static tdeck::AlphaResourceOwners owners{};
    if (pack.load(owners, report) != ESP_OK) { std::fprintf(stderr, "cannot load the resource pack\n"); return 2; }
    g_owners = &owners;

    test_loot_mechanism();
    test_loot_runtime();
    test_chest_lifecycle(false);
    test_chest_lifecycle(true);
    test_chest_negatives();
    test_door_lifecycle(false);
    test_door_lifecycle(true);
    test_door_negatives();

    std::printf("\nbatch23_vault_parity: %d/%d checks GREEN, %d RED\n", g_checks - g_failures, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
