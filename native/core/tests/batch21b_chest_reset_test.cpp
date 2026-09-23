// Batch 21B -- H-148: the 1988 chest refill in Lord British's castle basement.
//
// Reference derivation (this batch, straight from the shipped 1988 binaries):
//
//   CMDS.OVL:0x0552 `cmd_camp_holeup` is the (H)ole up command, reachable only
//   with the party standing on a bed tile 0xAB (ULTIMA.EXE:0x32b9
//   `cmp [bp-4],0xab / je 0x32c6`, else "Only in bed!" DS 0xa17a). Its clock
//   loop advances 10 minutes per tick (0x0647 `push 0x0a` -> kernel 0x4F7C)
//   and then, at 0x0677, calls kernel thunk 0x7A8E = TOWN.OVL:0x1694
//   `town_populate_npcs` -- INSIDE the loop (0x068d `je 0x634` jumps back).
//
//   TOWN.OVL:0x1694 is ONE routine covering both halves of the 1988 interior
//   actor table: 0x16a2-0x16b9 zeroes all 31 object slots (kernel 0x3A74 with
//   six zero fields) and every live NPC objIdx; 0x16c9-0x171b then re-places
//   every .NPC slot whose type byte is non-zero. A chest is .NPC type 1, and
//   TOWN.OVL:0x1726 `town_npc_place` seeds its +5 contents byte from the
//   constant at 0x1795 (`mov word [bp-6],0x1e`) without consulting any
//   persistence bit -- the type-1 branch jumps straight past the killed-NPC
//   bitmask test at 0x179c. Opening a chest (SJOG.OVL:0x112C
//   `open_chest_world`, 0x11d6-0x11e1) only zeroes the object slot; it records
//   nothing. So a looted chest is recreated, fully stocked, on the first tick.
//
//   The authored vault is real data: CASTLE.NPC record (17-1)&7 = 0 carries
//   type 0x01 in slots 23/24/25 at (16,21) (17,22) (13,23), all z = 0xFF (the
//   basement, runtime floor -1), plus a type 0x1e dead body in slot 28 at
//   (9,9). The same basement map carries two beds (0xAB) at (13,19) and
//   (16,19), four tiles from the chests, and the magically sealed doors
//   0x97/0x98 that a skull key unmagics.
//
// Native root cause: the port splits that single table into NpcActors and the
// quest-object pool. `bed_sleep_step` (rest.cpp) drove only the NPC half via
// RestServices::snap_npcs; nothing ran the object half, so chests never came
// back. These cases pin both halves and the things that must NOT reset.
#include "openu5/commands.h"
#include "openu5/quest_world.h"
#include "openu5/rest.h"
#include "openu5/world_commands.h"
#include <cstring>
#include <iostream>
#include <vector>

using namespace openu5;
namespace {
int checks = 0;
void check(bool ok, const char *what) {
    ++checks;
    if (!ok) {
        std::cerr << "batch21b check failed: " << what << "\n";
        std::exit(1);
    }
}

// --- The authored Lord British basement, byte-for-byte from CASTLE.NPC. -----
constexpr uint8_t kLocation = 17;
constexpr int16_t kBasement = -1;
constexpr uint8_t kBedX = 16, kBedY = 19; // basement bed, smallmaps.json loc 17 z -1
constexpr uint8_t kBedTile = 0xAB;

struct Objects {
    std::vector<QuestObject> values;
};
size_t obj_count(void *p) { return static_cast<Objects *>(p)->values.size(); }
QuestObject obj_read(void *p, size_t i) { return static_cast<Objects *>(p)->values[i]; }
bool obj_reserve(void *p, size_t n) {
    auto &v = static_cast<Objects *>(p)->values;
    v.reserve(v.size() + n);
    return true;
}
void obj_append(void *p, const QuestObject &o) { static_cast<Objects *>(p)->values.push_back(o); }
void obj_erase(void *p, size_t i) {
    auto &v = static_cast<Objects *>(p)->values;
    v.erase(v.begin() + ptrdiff_t(i));
}
void obj_write(void *p, size_t i, const QuestObject &o) { static_cast<Objects *>(p)->values[i] = o; }

struct Harness {
    GameState g{};
    TurnState t{};
    TravelState travel{};
    CommandState commands{};
    NpcActors actors{};
    NpcScanGrid scratch{};
    Objects objects{};
    QuestWorldServices quest{};
    RestServices rest{};
    std::vector<uint8_t> small;
    MapData map{};
    WorldData world{};
    std::vector<int> reloads;
    int snaps = 0;
    // CASTLE.NPC loc 17: three chests and the dead body, at their authored cells.
    NpcSlot slots[5]{
        {23, 1, 0, {0, 0, 0}, {16, 16, 16}, {21, 21, 21}, {255, 255, 255}, {0, 0, 0, 0}},
        {24, 1, 0, {0, 0, 0}, {17, 17, 17}, {22, 22, 22}, {255, 255, 255}, {0, 0, 0, 0}},
        {25, 1, 0, {0, 0, 0}, {13, 13, 13}, {23, 23, 23}, {255, 255, 255}, {0, 0, 0, 0}},
        {28, 30, 0, {0, 0, 0}, {9, 9, 9}, {9, 9, 9}, {255, 255, 255}, {0, 0, 0, 0}},
        {22, 0xB5, 0, {0, 0, 0}, {15, 15, 15}, {13, 13, 13}, {255, 255, 255}, {0, 0, 0, 0}},
    };
    NpcLocationData npc_data{kLocation, slots, 4, 0};
    CommandContext context{g, t, travel, commands, world};

    Harness() {
        small.assign(1024, 5);
        small[size_t(kBedY) * 32 + kBedX] = kBedTile;
        map = MapData{{kLocation, kBasement}, small.data(), small.size()};
        world = WorldData{nullptr, nullptr, 0, 0, &map, 1};
        g.position.map = {kLocation, kBasement};
        g.position.xy = {kBedX, kBedY};
        g.party.party_size = 1;
        g.party.character_count = 1;
        g.party.characters[0].status = 'G';
        // This fixture tests chest resets, not survival. Bed sleep now runs
        // original turn housekeeping, so give its sole member provisions.
        g.party.characters[0].current_hp = g.party.characters[0].max_hp = 500;
        g.food = 100;
        g.time.hour = 12;
        g.time.minute = 0;
        quest.context = &objects;
        quest.count = obj_count;
        quest.read = obj_read;
        quest.reserve = obj_reserve;
        quest.append = obj_append;
        quest.erase = obj_erase;
        quest.write = obj_write;
        rest.context = this;
        rest.snap_npcs = [](void *p) { ++static_cast<Harness *>(p)->snaps; };
        rest.occupied = [](void *, int32_t, int32_t, int32_t) { return false; };
        rest.cell_free = [](void *, int32_t, int32_t) { return true; };
        rest.karma_record = [](void *, int32_t) { return "\"TEST KARMA\""; };
        context.actors = &actors;
        context.npc_scratch = &scratch;
        context.npc_data = &npc_data;
        context.npc_data_count = 1;
        context.quest_world = &quest;
        context.rest_services = &rest;
        context.services = {this, [](void *, CommandEffect, EventSink) { return false; },
                            [](void *p, ReloadEffect e, uint8_t, EventSink) {
                                static_cast<Harness *>(p)->reloads.push_back(int(e));
                            },
                            [](void *, uint8_t) { return "\n\nTEST\n"; }};
    }
    size_t chests() const {
        size_t n = 0;
        for (const auto &o : objects.values)
            if (o.chest)
                ++n;
        return n;
    }
    size_t props() const {
        size_t n = 0;
        for (const auto &o : objects.values)
            if (o.prop)
                ++n;
        return n;
    }
    size_t plots() const {
        size_t n = 0;
        for (const auto &o : objects.values)
            if (o.plot)
                ++n;
        return n;
    }
    size_t loot_piles() const {
        size_t n = 0;
        for (const auto &o : objects.values)
            if (o.loot)
                ++n;
        return n;
    }
    bool chest_at(int x, int y) const {
        for (const auto &o : objects.values)
            if (o.chest && o.x == x && o.y == y && o.floor == kBasement && o.location == kLocation)
                return true;
        return false;
    }
    void enter() { hydrate_interior_objects(context, kLocation); }
    // Loot every chest through the production (O)pen path: stand north of the
    // chest and open southwards, exactly as a player would.
    void loot_all() {
        for (bool more = true; more;) {
            more = false;
            for (const auto &o : objects.values)
                if (o.chest) {
                    g.position.xy = {uint8_t(o.x), uint8_t(o.y - 1)};
                    Command cmd;
                    cmd.kind = CommandKind::Open;
                    cmd.direction = Direction::South;
                    execute_command(context, cmd);
                    more = true;
                    break;
                }
        }
        g.position.xy = {kBedX, kBedY}; // back to the bed for the hole up
    }
    Rand rand() {
        return {this, [](void *p, int32_t lo, int32_t hi) {
                    return static_cast<Harness *>(p)->g.rng.next(lo, hi).value;
                }};
    }
    ActionResult rest_hours(int32_t hours) {
        Command cmd;
        cmd.kind = CommandKind::Rest;
        cmd.hours = hours;
        return execute_command(context, cmd);
    }
};
} // namespace

int main() {
    // =====================================================================
    // H148-A -- initial chest state. The authored vault hydrates on entry.
    // =====================================================================
    {
        Harness h;
        h.enter();
        check(h.chests() == 3, "H148-A three authored chests hydrate in the LB basement");
        check(h.chest_at(16, 21) && h.chest_at(17, 22) && h.chest_at(13, 23),
              "H148-A at the CASTLE.NPC slot 23/24/25 coordinates");
        check(h.props() == 1, "H148-A the authored dead body (slot 28) hydrates too");
        for (const auto &o : h.objects.values)
            if (o.chest)
                check(!o.trapped, "H148-A an authored interior chest is seeded untrapped");
    }

    // =====================================================================
    // H148-B -- loot depletion. (O)pen removes the chest object.
    // =====================================================================
    {
        Harness h;
        h.enter();
        h.loot_all();
        check(h.chests() == 0, "H148-B every opened chest is removed from the object pool");
        check(h.props() == 1, "H148-B looting chests does not disturb the authored prop");
    }

    // =====================================================================
    // H148-C -- the minimum original trigger. One hour of (H)ole up on the
    // basement bed re-seeds the vault. This is the RED case: pre-fix the
    // chests stay gone because only the NPC half of town_populate_npcs ran.
    // =====================================================================
    {
        Harness h;
        h.enter();
        h.loot_all();
        check(h.chests() == 0, "H148-C precondition: the vault is empty before resting");
        const auto r = h.rest_hours(1);
        check(r.status == CommandStatus::Success || r.status == CommandStatus::NoOp,
              "H148-C the one-hour hole up executes");
        check(h.snaps == 6, "H148-C six 10-minute ticks per hour, each running the routine");
        check(h.chests() == 3, "H148-C ** all three chests are back after the minimum hole up **");
        check(h.chest_at(16, 21) && h.chest_at(17, 22) && h.chest_at(13, 23),
              "H148-C and at exactly their authored cells");
        check(h.props() == 1, "H148-C the prop is re-placed once, not duplicated");
    }

    // =====================================================================
    // H148-D -- repeated farming. The 1988 loop has no cooldown and no
    // once-only bit, so loot -> rest -> loot -> rest repeats indefinitely.
    // =====================================================================
    {
        Harness h;
        h.enter();
        for (int cycle = 0; cycle < 3; ++cycle) {
            h.loot_all();
            check(h.chests() == 0, "H148-D the vault empties each cycle");
            h.rest_hours(1);
            check(h.chests() == 3, "H148-D and refills each cycle -- no cooldown, no once-only bit");
        }
        check(h.props() == 1, "H148-D repeated resets never accumulate duplicate props");
    }

    // =====================================================================
    // H148-E -- door / lock state. town_populate_npcs writes only the object
    // register; the map buffer that holds a skull-key-unmagicked door is
    // reloaded only by town_load_town_map (map entry / ladder). So holing up
    // must not emit any effect that would relock or reload the map.
    // =====================================================================
    {
        Harness h;
        h.enter();
        h.reloads.clear();
        h.rest_hours(2);
        for (auto e : h.reloads) {
            check(e != int(ReloadEffect::ClearTerrain),
                  "H148-E hole up never clears volatile terrain (the unmagicked door stays open)");
            check(e != int(ReloadEffect::ResetDoors), "H148-E hole up never resets door timers");
            check(e != int(ReloadEffect::HydrateInterior),
                  "H148-E the reset is the in-loop routine, not a map-entry reload");
        }
    }

    // =====================================================================
    // H148-E2 -- ordering inside one tick. CMDS.OVL runs the repopulate at
    // 0x0677 and only then asks `find_object_at_xy` (0x0688 `call 0x770e`)
    // whether the party's own cell is occupied, 17 bytes later in the SAME
    // iteration. So an object the reset drops onto the bed throws the party
    // out ("Thrown out of bed!", DS 0x422a). This pins the order and pins that
    // the occupancy probe still reaches the host's view of the world.
    // =====================================================================
    {
        Harness h;
        // A synthetic .NPC chest authored ON the bed cell itself.
        h.slots[4] = {22, 1, 0, {0, 0, 0}, {kBedX, kBedX, kBedX}, {kBedY, kBedY, kBedY},
                      {255, 255, 255}, {0, 0, 0, 0}};
        h.npc_data.count = 5;
        h.rest.occupied = [](void *p, int32_t x, int32_t y, int32_t z) {
            auto &n = *static_cast<Harness *>(p);
            for (const auto &o : n.objects.values)
                if (o.x == x && o.y == y && o.floor == z && o.location == n.g.position.map.location)
                    return true;
            return false;
        };
        h.enter();
        check(h.chests() == 4, "H148-E2 precondition: four chests including one on the bed");
        h.loot_all();
        check(h.chests() == 0, "H148-E2 precondition: the bed cell is clear before sleeping");
        h.rest_hours(9);
        check(h.snaps == 1, "H148-E2 the very first tick ends the sleep, not the ninth hour");
        check(h.chests() == 4, "H148-E2 because the reset ran first and re-placed the bed chest");
    }

    // =====================================================================
    // H148-F -- negative controls. Things the 1988 routine does NOT reset.
    // =====================================================================
    {
        // F1: camping outdoors. kernel 0x3C9A never calls TOWN.OVL:0x1694 --
        // the census of thunk 0x7A8E finds exactly one caller, CMDS.OVL:0x0677.
        Harness h;
        h.enter();
        h.loot_all();
        h.g.position.map = {0, 0};
        h.small[size_t(kBedY) * 32 + kBedX] = 5; // no bed underfoot
        h.rest_hours(1);
        check(h.chests() == 0, "H148-F1 camping outdoors does not refill interior chests");
    }
    {
        // F2: a quest/plot object already taken stays taken across the reset.
        Harness h;
        h.npc_data.count = 5; // adds the crown slot
        h.g.quest.artifacts[1] = 1; // the crown is already in the pack
        h.enter();
        check(h.plots() == 0, "H148-F2 a taken plot artifact does not hydrate");
        h.rest_hours(1);
        check(h.plots() == 0, "H148-F2 and the hole-up reset does not resurrect it either");
        check(h.chests() == 3, "H148-F2 while the chests still refill alongside it");
    }
    {
        // F2b: positive control for F2 -- an UNTAKEN plot artifact does come
        // back, so F2 is measuring the taken gate and not a dead code path.
        Harness h;
        h.npc_data.count = 5;
        h.enter();
        check(h.plots() == 1, "H148-F2b control: an untaken plot artifact hydrates");
        h.rest_hours(1);
        check(h.plots() == 1, "H148-F2b control: and is re-placed exactly once by the reset");
    }
    {
        // F3: a search object (the once-only (S)earch table) is not in the
        // .NPC register at all, so the reset can never re-arm one.
        Harness h;
        h.enter();
        QuestObject found;
        found.location = kLocation;
        found.floor = kBasement;
        found.x = 10;
        found.y = 10;
        found.search = true;
        h.objects.values.push_back(found);
        h.rest_hours(1);
        size_t searches = 0;
        for (const auto &o : h.objects.values)
            if (o.search)
                ++searches;
        check(searches == 0,
              "H148-F3 the reset wipes the whole object register -- it never re-arms a search find");
    }
    {
        // F4: loot piles dropped on the floor share the 1988 object register,
        // so the wipe half of town_populate_npcs destroys them. Faithful, and
        // the reason the farm needs a (G)et before the rest.
        Harness h;
        h.enter();
        h.loot_all();
        const auto piles = h.loot_piles();
        check(piles > 0, "H148-F4 precondition: opening the chests left loot on the floor");
        h.rest_hours(1);
        check(h.loot_piles() == 0,
              "H148-F4 the wipe half destroys un-collected floor loot, as the 1988 register does");
        check(h.chests() == 3, "H148-F4 and the chests are back in the same pass");
    }

    // =====================================================================
    // H148-G -- persistence. The 1988 object register and live NPC table both
    // live inside the save window (DS 0x5C5A / 0x5F5E, both within
    // 0x55A6..0x6605), so a save taken after the reset carries the refilled
    // vault. In the port the pool IS that state: no re-hydration is needed for
    // the chests to be openable again, and a redundant hydrate is idempotent.
    // =====================================================================
    {
        Harness h;
        h.enter();
        h.loot_all();
        h.rest_hours(1);
        check(h.chests() == 3, "H148-G the refilled vault is pool state, not a render artifact");
        h.loot_all();
        check(h.chests() == 0, "H148-G and the refilled chests are really openable again");
        h.rest_hours(1);
        h.enter(); // a redundant map-entry hydrate on top of the reset
        check(h.chests() == 3 && h.props() == 1, "H148-G re-entry on top of a reset does not duplicate");
    }

    std::cout << "batch21b chest reset: " << checks << " checks passed\n";
    return 0;
}
