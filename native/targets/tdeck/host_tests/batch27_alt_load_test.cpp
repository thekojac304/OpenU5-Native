// Batch 27 -- H-164: Alt+L must restore what System Menu -> Load / Save
// Management -> Continue Latest restores.
//
// Both routes read the SAME generation through the SAME AlphaSaveService::load
// (alpha_save.cpp: candidate() -> restore_candidate(), transactional: the live
// GameState/TurnState/CommandState/outdoor/terrain/actors/document are replaced
// only when the whole restore succeeded). What differs is what AlphaRuntime does
// AFTER a successful read:
//
//   System Menu / frontend Continue:  service_*_intent() -> synchronize_loaded_world()
//   Alt+L (DeviceShortcut::Load):     a hand-copied subset of it in handle()
//
// The subset (Batch 24's door reset, the NPC re-entry, the terrain refresh)
// never reached the sidecar-owned state that synchronize_loaded_world() alone
// restores: the loose-object pool (R-14, "worldObjects") and the dungeon
// session (R-15 / Batch 26, "dungeon"). Batch 26 observation Q showed the
// dungeon half on host. 1988 has one load: INTRO.OVL 0x0EB4 reads the whole
// window back and ULTIMA.EXE 0x00DB resumes into it (see batch26_dungeon_save
// for the disassembly trail); a keyboard shortcut is a device affordance for
// that same load, not a second, lighter one.
//
// Same seam as Batches 24-26: the real AlphaRuntime, the shipped pack, raw
// keys and trackball, and alpha_save_memory_host_stub.cpp (one in-memory
// generation running alpha_save.cpp's own serialization chain).
#include "../main/alpha_runtime.h"

#include "openu5/dungeon.h"
#include "openu5/quest_state.h"
#include "openu5/world_commands.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace tdeck {
void host_memory_save_forget_for_test();
void host_memory_save_damage_for_test();
} // namespace tdeck

using namespace openu5;
using tdeck::RawInputKind;

namespace {

int g_failures = 0, g_checks = 0;
bool expect(bool ok, const char *id, const char *what) {
    ++g_checks;
    std::printf("  %-5s %-5s %s\n", ok ? "GREEN" : "RED", id, what);
    if (!ok) ++g_failures;
    return ok;
}

constexpr uint8_t kDeceit = 33, kCastle = 17;
constexpr int16_t kBasement = -1;
struct Cell { int x, y; };
constexpr Cell kChests[3] = {{16, 21}, {17, 22}, {13, 23}};  // Lord British's basement (Batch 22)

const tdeck::AlphaResourceOwners *g_owners = nullptr;
size_t g_dungeon_count = 0;

bool same(const DungeonState &a, const DungeonState &b) {
    return a.active == b.active && a.pos.dungeon == b.pos.dungeon && a.pos.floor == b.pos.floor &&
           a.pos.x == b.pos.x && a.pos.y == b.pos.y && a.pos.facing == b.pos.facing &&
           !std::memcmp(a.cells, b.cells, sizeof(a.cells)) && !std::memcmp(a.revealed, b.revealed, sizeof(a.revealed)) &&
           a.wanderer.bank == b.wanderer.bank && a.wanderer.type == b.wanderer.type && a.wanderer.x == b.wanderer.x &&
           a.wanderer.y == b.wanderer.y && a.wanderer.floor == b.wanderer.floor && a.wanderer.attr == b.wanderer.attr &&
           a.wanderer.hidden == b.wanderer.hidden && a.wanderer.prev_x == b.wanderer.prev_x &&
           a.wanderer.prev_y == b.wanderer.prev_y;
}
bool same(const QuestObject &a, const QuestObject &b) {
    return a.location == b.location && a.floor == b.floor && a.x == b.x && a.y == b.y && a.tile == b.tile &&
           a.plot_z == b.plot_z && a.item == b.item && a.plot == b.plot && a.shadowlord == b.shadowlord &&
           a.search == b.search && a.loot == b.loot && a.item_id == b.item_id && a.quality == b.quality &&
           a.chest == b.chest && a.prop == b.prop && a.contents == b.contents && a.trapped == b.trapped &&
           a.slot == b.slot && a.hull == b.hull && a.skiffs == b.skiffs && a.ship == b.ship && a.torch == b.torch;
}
bool same(const std::vector<QuestObject> &a, const std::vector<QuestObject> &b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (!same(a[i], b[i])) return false;
    return true;
}
void describe(const DungeonState &d, const char *tag) {
    std::printf("         %s: active=%d dungeon=%u floor=%u (%u,%u) facing=%d quick=%u\n", tag, d.active,
                unsigned(d.pos.dungeon), unsigned(d.pos.floor), unsigned(d.pos.x), unsigned(d.pos.y),
                int(d.pos.facing), unsigned(d.quickness_toggle));
}
void describe(const std::vector<QuestObject> &pool, const char *tag) {
    std::printf("         %s: %zu objects\n", tag, pool.size());
    for (const auto &o : pool)
        std::printf("           loc=%d floor=%d (%d,%d) tile=%d chest=%d loot=%d item=%d/%d\n", o.location, o.floor,
                    o.x, o.y, o.tile, o.chest, o.loot, o.item_id, o.quality);
}

struct Harness {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    int64_t clock_us = 0;
    size_t mark = 0;
    Harness() {
        tdeck::AlphaRuntime::HostTestFixture hf;
        hf.world = g_owners->world;
        hf.location_x = g_owners->location_x; hf.location_y = g_owners->location_y;
        hf.location_count = g_owners->location_count;
        hf.npc_locations = g_owners->npc_locations;
        hf.dungeons = g_owners->dungeons; hf.dungeon_count = g_dungeon_count;
        hf.pack = g_owners;
        rt->attach_host_test_fixture(hf);
        auto &g = rt->game();
        g.party.character_count = g.party.party_size = 1;
        auto &ch = g.party.characters[0];
        std::snprintf(ch.name, sizeof(ch.name), "Avatar");
        ch.current_hp = ch.max_hp = 900; ch.status = 'G'; ch.party_status = 0; ch.character_class = 'A';
        ch.dexterity = 30; ch.strength = 30;
        g.party.active_character = 255;
        g.time.hour = 12; g.time.minute = 0;
        g.torch_turns = 500; g.torches = 5; g.skull_keys = 5; g.karma = 50; g.gold = 321;
        g.position.map = {0, 0};
    }
    GameState &g() { return rt->game(); }
    CommandContext &ctx() { return rt->command_context_for_test(); }
    const DungeonState &d() const { return rt->dungeon_state(); }
    const std::vector<QuestObject> &pool() const { return rt->objects_for_test(); }
    UiMode mode() const { return rt->ui()->mode(); }

    bool raw_key(uint8_t code, bool alt = false) {
        tdeck::RawInputEvent raw{};
        raw.kind = RawInputKind::Keyboard; raw.code = code; raw.modifiers.alt = alt;
        raw.transition = tdeck::KeyTransition::Pressed; raw.timestamp_us = (clock_us += 100000);
        return rt->handle(raw);
    }
    bool key(uint8_t code) { return raw_key(code); }
    bool ball(RawInputKind kind) {
        tdeck::RawInputEvent raw{}; raw.kind = kind; raw.timestamp_us = (clock_us += 100000); return rt->handle(raw);
    }
    void forward() { ball(RawInputKind::TrackballUp); }
    void turn_left() { ball(RawInputKind::TrackballLeft); }
    void turn_right() { ball(RawInputKind::TrackballRight); }

    // The two load frontends under comparison, and the two saves.
    void alt_save() { raw_key('s', true); }
    void alt_load() { raw_key('l', true); }                                // DeviceShortcut::Load
    void menu_save() { raw_key('m', true); ball(RawInputKind::TrackballDown); key('\r'); raw_key('m', true); }
    void menu_load() {                                                    // Load / Save Management -> Continue Latest
        raw_key('m', true); ball(RawInputKind::TrackballDown); ball(RawInputKind::TrackballDown);
        key('\r'); key('\r');
    }

    void set_mark() { mark = rt->ui()->transcript_size(); }
    bool saw(const char *needle) const {
        for (size_t i = mark; i < rt->ui()->transcript_size(); ++i)
            if (const auto *b = rt->ui()->transcript_at(i); b && std::strstr(b->text, needle)) return true;
        return false;
    }

    bool enter_deceit() {
        g().position.map = {0, 0};
        g().position.xy = {g_owners->location_x[kDeceit - 1], g_owners->location_y[kDeceit - 1]};
        set_quest_flag(g().quest, QuestFlag::Word33);
        key('e');
        return d().active && d().pos.dungeon == kDeceit && d().pos.floor == 0 && d().pos.x == 1 && d().pos.y == 1;
    }
    // Deceit floor 0 from the (1,1) ladder onto the two (3,2) traps: floor 2, (3,2), North.
    bool fall_to_floor_two() {
        forward(); forward(); turn_left(); forward(); forward(); turn_left(); forward();
        return d().active && d().pos.floor == 2 && d().pos.x == 3 && d().pos.y == 2 &&
               d().pos.facing == DungeonFacing::North;
    }
    bool traps_sprung() const {
        return d().active && d().cells[0 * 64 + 2 * 8 + 3] == 0x60 && d().cells[1 * 64 + 2 * 8 + 3] == 0x60;
    }
    bool enter_castle() {
        g().position.map = {0, 0};
        g().position.xy = {g_owners->location_x[kCastle - 1], g_owners->location_y[kCastle - 1]};
        key('e');
        return g().position.map.location == kCastle && g().position.map.floor == 0;
    }
    bool klimb_down() {
        g().position.xy = {1, 1};
        key('k');
        return g().position.map.location == kCastle && g().position.map.floor == kBasement;
    }
    ActionResult run(CommandKind k, Direction dir) {
        Command c; c.kind = k; c.direction = dir; c.has_direction = true;
        return execute_command(ctx(), c);
    }
    void open_chest(const Cell &c) {
        g().position.xy = {uint8_t(c.x), uint8_t(c.y + 1)};
        run(CommandKind::Open, Direction::North);
    }
    size_t chests_at(const Cell &c) const {
        size_t n = 0;
        for (const auto &o : pool())
            if (o.chest && o.location == kCastle && o.floor == kBasement && o.x == c.x && o.y == c.y) ++n;
        return n;
    }
    size_t loot_at(const Cell &c) const {
        size_t n = 0;
        for (const auto &o : pool())
            if (o.loot && o.location == kCastle && o.floor == kBasement && o.x == c.x && o.y == c.y) ++n;
        return n;
    }
};

// Everything a player (or the next command) can see after a load. Deliberately
// the observable game/runtime state, not every private member.
struct Snapshot {
    WorldPosition pos{};
    GameTime time{};
    uint32_t gold = 0;
    int karma = 0;
    DungeonState dungeon{};
    std::vector<QuestObject> pool;
    bool ctx_dungeon = false, ctx_combat = false, combat_live = false;
    UiMode mode = UiMode::Exploration, base_mode = UiMode::Exploration;
    size_t actors = 0;
    std::vector<int> actor_xyz;
    int door_turns = 0;
};
Snapshot snap(Harness &h) {
    Snapshot s;
    s.pos = h.g().position; s.time = h.g().time; s.gold = h.g().gold; s.karma = h.g().karma;
    s.dungeon = h.d(); s.pool = h.pool();
    s.ctx_dungeon = h.rt->command_context().dungeon; s.ctx_combat = h.rt->command_context().combat;
    s.combat_live = h.rt->combat_state().initialized && !h.rt->combat_state().ended;
    s.mode = h.mode(); s.base_mode = h.rt->ui()->base_mode();
    s.actors = h.rt->actors().count;
    for (size_t i = 0; i < s.actors; ++i) {
        const auto &a = h.rt->actors().actors[i];
        s.actor_xyz.insert(s.actor_xyz.end(), {a.x, a.y, a.z, a.state});
    }
    s.door_turns = int(h.rt->commands().door.turns);
    return s;
}
// Field-by-field so a RED names the field that diverged.
bool equivalent(const Snapshot &a, const Snapshot &b, const char *tag) {
    bool ok = true;
    auto field = [&](bool eq, const char *name) {
        if (!eq) { std::printf("         %s: differs in %s\n", tag, name); ok = false; }
    };
    field(a.pos.map.location == b.pos.map.location && a.pos.map.floor == b.pos.map.floor && a.pos.xy.x == b.pos.xy.x &&
              a.pos.xy.y == b.pos.xy.y,
          "GameState position (map/floor/x/y)");
    field(a.time.year == b.time.year && a.time.month == b.time.month && a.time.day == b.time.day &&
              a.time.hour == b.time.hour && a.time.minute == b.time.minute,
          "clock");
    field(a.gold == b.gold && a.karma == b.karma, "gold/karma");
    field(a.dungeon.active == b.dungeon.active, "dungeon active");
    field(a.dungeon.pos.dungeon == b.dungeon.pos.dungeon && a.dungeon.pos.floor == b.dungeon.pos.floor &&
              a.dungeon.pos.x == b.dungeon.pos.x && a.dungeon.pos.y == b.dungeon.pos.y &&
              a.dungeon.pos.facing == b.dungeon.pos.facing,
          "dungeon id/level/cell/facing");
    field(same(a.dungeon, b.dungeon), "dungeon map/reveal/wanderer");
    field(a.dungeon.quickness_toggle == b.dungeon.quickness_toggle, "Rel Tym toggle");
    field(same(a.pool, b.pool), "loose-object pool");
    field(a.ctx_dungeon == b.ctx_dungeon && a.ctx_combat == b.ctx_combat && a.combat_live == b.combat_live,
          "command context (dungeon/combat)");
    field(a.mode == b.mode && a.base_mode == b.base_mode, "UI mode / base mode");
    field(a.actors == b.actors && a.actor_xyz == b.actor_xyz, "NPC actors");
    field(a.door_turns == b.door_turns, "open-door tracker");
    return ok;
}

// ---------------------------------------------------------------------------
// X: a load that fails must not synchronize anything. Runs FIRST, while the
// in-memory store is still empty.
// ---------------------------------------------------------------------------
void test_failed_loads() {
    std::printf("X  failed loads leave the live world exactly as it was (both frontends)\n");
    tdeck::host_memory_save_forget_for_test();
    Harness h;
    expect(h.enter_deceit() && h.fall_to_floor_two(), "X0", "precondition: live in Deceit floor 2 (3,2), no save exists");
    h.turn_right();
    const Snapshot live = snap(h);
    h.set_mark();
    h.alt_load();
    expect(h.saw("No valid save"), "X1a", "missing save: Alt+L reports \"No valid save\"");
    expect(equivalent(snap(h), live, "missing/Alt+L"), "X1b", "missing save: Alt+L changed nothing (session, pool, mode, clock)");
    h.set_mark();
    h.menu_load();
    expect(h.saw("No valid save") && equivalent(snap(h), live, "missing/menu"), "X1c",
           "missing save: Continue Latest reports it and changes nothing either");
    h.raw_key('m', true);                                                 // close the menu the failed load left open

    // A fresh runtime: a real save, the play after it, then the stored copy is damaged.
    Harness f;
    f.enter_deceit(); f.fall_to_floor_two();
    f.set_mark();
    f.menu_save();
    expect(f.saw("Save complete"), "X2", "precondition: a real save on Deceit floor 2 (3,2)");
    f.turn_right(); f.forward();
    f.g().gold = 999;
    tdeck::host_memory_save_damage_for_test();
    const Snapshot before = snap(f);
    f.set_mark();
    f.alt_load();
    expect(f.saw("No valid save"), "X2a", "damaged save: Alt+L reports \"No valid save\"");
    expect(f.g().gold == 999 && equivalent(snap(f), before, "damaged/Alt+L"), "X2b",
           "** damaged save: Alt+L rejected it and synchronized nothing (gold 999 kept, session and pool untouched) **");
    f.set_mark();
    f.menu_load();
    expect(f.saw("No valid save") && equivalent(snap(f), before, "damaged/menu"), "X2c",
           "damaged save: Continue Latest rejects it the same way");
    f.raw_key('m', true);
    tdeck::host_memory_save_forget_for_test();
}

// ---------------------------------------------------------------------------
// A-D, G: a dungeon save, loaded by Alt+L.
// ---------------------------------------------------------------------------
DungeonState g_saved_session;

void test_same_session() {
    std::printf("S  save on Deceit floor 2, turn/move/Rel Tym after it, Alt+L in the same session\n");
    Harness h;
    expect(h.enter_deceit() && h.fall_to_floor_two() && h.traps_sprung(), "S0",
           "precondition: Deceit floor 2 (3,2) facing N, the floor-0/1 (3,2) traps sprung (0x61 -> 0x60)");
    h.menu_save();
    g_saved_session = h.d();
    const auto minute = h.g().time.minute;
    h.turn_right(); h.forward(); h.turn_right(); h.forward();
    h.rt->turn().time_spell = 'Q';                                        // Rel Tym: DUNGEON 0x0F15/0x0F25 toggle
    h.turn_left();
    expect(!same(h.d(), g_saved_session) && h.d().quickness_toggle == 1 && h.g().time.minute != minute, "S1",
           "precondition: after the save the party moved and turned, time passed, and the Rel Tym toggle is 1");
    h.g().gold = 999;
    h.set_mark();
    h.alt_load();
    expect(h.saw("Load complete") && h.g().gold == 321 && h.g().time.minute == minute, "S2",
           "Alt+L loaded: \"Load complete\", gold 999 -> 321, clock back to the save");
    if (!expect(h.d().active && h.d().pos.dungeon == kDeceit, "H164-A1", "the dungeon session is active and it is Deceit"))
        describe(h.d(), "loaded");
    if (!expect(h.d().pos.floor == 2 && h.d().pos.x == 3 && h.d().pos.y == 2 && h.d().pos.facing == DungeonFacing::North &&
                    same(h.d(), g_saved_session),
                "H164-B1", "** Alt+L puts the party back on floor 2 (3,2) facing N with the saved map, reveal and wanderer **")) {
        describe(g_saved_session, "saved");
        describe(h.d(), "loaded");
    }
    expect(h.d().active && h.d().quickness_toggle == 0, "H164-G",
           "** the Rel Tym toggle is 0 after Alt+L, as after any load (DUNGEON 0x0E40) **");
    expect(h.ctx().dungeon && h.mode() == UiMode::Dungeon && h.rt->ui()->base_mode() == UiMode::Dungeon, "H164-C1",
           "dungeon context and Dungeon UI mode the moment Alt+L returns");
    h.turn_right();
    expect(h.d().active && h.d().pos.floor == 2 && h.d().pos.x == 3 && h.d().pos.y == 2 &&
               h.d().pos.facing == DungeonFacing::East,
           "H164-C2", "the very next trackball input turns the RESTORED party (N -> E) where the save left it");
}

void test_after_leaving() {
    std::printf("K  save at the Deceit floor-0 ladder, (K)limb out to Britannia, Alt+L\n");
    Harness h;
    expect(h.enter_deceit(), "K0", "precondition: Deceit floor 0 (1,1)");
    const DungeonState saved = h.d();
    h.alt_save();
    h.key('k');
    expect(!h.d().active && h.g().position.map.location == 0 && h.mode() != UiMode::Dungeon, "K1",
           "precondition: (K)limb left the dungeon; on the surface");
    h.alt_load();
    if (!expect(h.d().active && same(h.d(), saved), "H164-A2",
                "** Alt+L resumes underground at the saved cell, not at the surface entrance **"))
        describe(h.d(), "loaded");
    expect(h.ctx().dungeon && h.mode() == UiMode::Dungeon, "H164-C3", "and the device is in the dungeon loop at once");
}

void test_power_cycle() {
    std::printf("P  power cycle: a fresh runtime on the surface, Alt+L the floor-2 save\n");
    {                                                                     // K saved last; store S's floor-2 save again
        Harness s;
        s.enter_deceit(); s.fall_to_floor_two();
        s.menu_save();
        g_saved_session = s.d();
    }
    Harness h;
    h.g().position.xy = {g_owners->location_x[kCastle - 1], g_owners->location_y[kCastle - 1]};
    expect(!h.d().active, "P0", "precondition: nothing in RAM -- no dungeon session");
    h.alt_load();
    if (!expect(h.d().active && h.d().pos.dungeon == kDeceit && h.d().pos.floor == 2 && h.d().pos.x == 3 &&
                    h.d().pos.y == 2 && h.d().pos.facing == DungeonFacing::North,
                "H164-B2", "** the Deceit floor-2 (3,2) N session comes back from storage alone **"))
        describe(h.d(), "loaded");
    expect(h.traps_sprung() && same(h.d(), g_saved_session), "H164-D",
           "** the sprung traps stay sprung (0x60) and map/reveal/wanderer are the saved ones, not DUNGEON.DAT **");
    expect(h.ctx().dungeon && h.mode() == UiMode::Dungeon, "H164-C4", "Dungeon UI mode the moment Alt+L returns");
    h.turn_right();
    expect(h.d().active && h.d().pos.facing == DungeonFacing::East && h.d().pos.floor == 2, "H164-C5",
           "the next trackball input turns the restored party");
}

// ---------------------------------------------------------------------------
// E: the loose-object pool (R-14) through Alt+L.
// ---------------------------------------------------------------------------
std::vector<QuestObject> g_saved_pool;

void test_loose_objects() {
    std::printf("E  castle basement: one chest opened, save, two more opened, Alt+L\n");
    Harness h;
    expect(h.enter_castle() && h.klimb_down(), "E0", "precondition: Lord British's basement");
    h.open_chest(kChests[0]);
    const size_t piles = h.loot_at(kChests[0]);
    h.alt_save();
    g_saved_pool = h.pool();
    h.open_chest(kChests[1]);
    h.open_chest(kChests[2]);
    expect(piles > 0 && !h.chests_at(kChests[1]) && !h.chests_at(kChests[2]), "E1",
           "precondition: (16,21) opened before the save, (17,22) and (13,23) after it");
    h.alt_load();
    if (!expect(same(h.pool(), g_saved_pool), "H164-E1", "** after Alt+L the object pool is exactly the saved one **")) {
        describe(g_saved_pool, "saved");
        describe(h.pool(), "loaded");
    }
    expect(h.chests_at(kChests[1]) == 1 && h.chests_at(kChests[2]) == 1 && !h.loot_at(kChests[1]) &&
               !h.loot_at(kChests[2]) && h.loot_at(kChests[0]) == piles,
           "H164-E2", "(17,22) and (13,23) closed again once each; (16,21) open with its loot");

    std::printf("E' power cycle: a fresh runtime with a different pool live, Alt+L\n");
    Harness f;
    expect(f.enter_castle() && f.klimb_down(), "E3", "precondition: fresh runtime in the basement");
    f.open_chest(kChests[2]);
    f.alt_load();
    if (!expect(same(f.pool(), g_saved_pool) && f.chests_at(kChests[2]) == 1 && !f.loot_at(kChests[2]), "H164-E3",
                "** no pre-load object survives Alt+L; the pool is the saved one **")) {
        describe(g_saved_pool, "saved");
        describe(f.pool(), "loaded");
    }
}

// ---------------------------------------------------------------------------
// F: surface saves.
// ---------------------------------------------------------------------------
void test_surface() {
    std::printf("F  surface saves through Alt+L\n");
    Harness h;
    h.g().position.xy = {g_owners->location_x[kCastle - 1], uint8_t(g_owners->location_y[kCastle - 1] + 2)};
    const WorldPosition saved = h.g().position;
    h.alt_save();
    h.g().position.xy = {uint8_t(saved.xy.x + 3), saved.xy.y};
    h.g().gold = 999;
    h.alt_load();
    expect(h.g().position.map.location == 0 && h.g().position.xy.x == saved.xy.x && h.g().position.xy.y == saved.xy.y &&
               h.g().gold == 321 && !h.d().active && !h.ctx().dungeon && h.mode() == UiMode::Exploration,
           "H164-F1", "an ordinary surface save loads through Alt+L as before: position, gold, Exploration mode");
    h.enter_deceit();
    h.forward();
    expect(h.d().active, "F2", "precondition: underground after the surface save");
    h.alt_load();
    if (!expect(!h.d().active && !h.ctx().dungeon && h.mode() == UiMode::Exploration &&
                    h.g().position.xy.x == saved.xy.x && h.g().position.xy.y == saved.xy.y,
                "H164-F2", "** Alt+L of a surface save taken underground ends the live dungeon session **"))
        describe(h.d(), "loaded");
}

// ---------------------------------------------------------------------------
// Q: one serialized save, both frontends, compared.
// ---------------------------------------------------------------------------
void compare_fresh(const char *id, const char *what) {
    Harness a, b;
    a.alt_load();
    b.menu_load();
    expect(equivalent(snap(a), snap(b), "Alt+L vs Continue Latest"), id, what);
}

void test_equivalence() {
    std::printf("Q  the same stored generation through Alt+L and through System Menu Continue Latest\n");
    {
        Harness s;
        s.enter_deceit(); s.fall_to_floor_two();
        s.menu_save();
    }
    compare_fresh("Q1", "** dungeon save, two fresh runtimes: every observable field identical **");
    {
        Harness s;
        s.enter_castle(); s.klimb_down(); s.open_chest(kChests[0]);
        s.menu_save();
    }
    compare_fresh("Q2", "** castle-basement save (objects, NPCs), two fresh runtimes: identical **");

    // Same runtime, live state perturbed differently before each load.
    Harness h;
    h.enter_deceit(); h.fall_to_floor_two();
    h.menu_save();
    h.turn_right(); h.forward(); h.g().gold = 999;
    h.alt_load();
    const Snapshot via_alt = snap(h);
    h.turn_left(); h.turn_left(); h.forward(); h.g().gold = 5;
    h.menu_load();
    const Snapshot via_menu = snap(h);
    expect(equivalent(via_alt, via_menu, "same runtime"), "Q3",
           "** one runtime: Alt+L and then Continue Latest land on the identical state **");
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: batch27_alt_load_test <openu5-alpha1-resources.bin>\n");
        return 2;
    }
    tdeck::AlphaResourcePack pack;
    tdeck::AlphaResourceReport report{};
    if (pack.open(argv[1], report) != ESP_OK) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    static tdeck::AlphaResourceOwners owners{};
    if (pack.load(owners, report) != ESP_OK) { std::fprintf(stderr, "cannot load the resource pack\n"); return 2; }
    g_owners = &owners;
    g_dungeon_count = report.dungeon_count;

    test_failed_loads();
    test_same_session();
    test_after_leaving();
    test_power_cycle();
    test_loose_objects();
    test_surface();
    test_equivalence();

    std::printf("\nbatch27_alt_load: %d/%d checks GREEN, %d RED\n", g_checks - g_failures, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
