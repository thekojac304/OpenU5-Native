// Batch 28 -- H-166: a save generation must pass the SAME semantic checks the
// load will apply before it may replace the live world.
//
// A generation is one 1988 save window (CAST2.OVL:0x10FE writes SAVED.GAM from
// 0x55A6 in one piece; INTRO.OVL:0x0EB4 reads it back whole). The dungeon
// session (g_location/floor/x/y 0x5893-0x5897, facing 0x6603, g_dng_map 0x595A)
// and the object register (0x5C5A) are inside that window. On the T-Deck they
// ride the JSON sidecar ("dungeon", "worldObjects"; A-14), and until Batch 28
// they were decoded only AFTER the generation had been committed
// (synchronize_loaded_world), with a "drop to no session / empty pool"
// fallback. The generation gate (alpha_save_generation.cpp, used by
// alpha_save.cpp's load, load_slot, inspect and post-write self-check) checked
// the CRC, the parse, and the gameplay/terrain/NPC owners only. So a
// well-formed, CRC-consistent newest generation whose sidecar the runtime
// cannot represent was chosen over a good older generation, and the world
// that came back was a mixture no save ever held.
//
// Seam: the real AlphaRuntime and the shipped pack, raw keys and trackball,
// and alpha_save_memory_host_stub.cpp -- two in-memory generation slots whose
// CRC, parse, semantic check, choice and fallback are the PRODUCTION
// alpha_save_generation.cpp. host_memory_save_edit_for_test() rewrites one
// generation's sidecar.gameState and re-seals its CRC: a well-formed,
// CRC-consistent generation whose content is wrong.
//
// Fixture staging, marked where used: a one-member party; standing on the
// Deceit entrance with its Word known (as batch26/27); loose objects appended
// through the SAME QuestWorldServices the runtime's commands use.
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
bool host_memory_save_edit_for_test(bool newest, void (*edit)(openu5::save::Json &game_state));
int host_memory_save_generations_for_test();
} // namespace tdeck

using namespace openu5;
using tdeck::RawInputKind;
using save::Json;

namespace {

int g_failures = 0, g_checks = 0;
bool expect(bool ok, const char *id, const char *what) {
    ++g_checks;
    std::printf("  %-5s %-5s %s\n", ok ? "GREEN" : "RED", id, what);
    if (!ok) ++g_failures;
    return ok;
}

constexpr uint8_t kDeceit = 33, kCastle = 17;
const tdeck::AlphaResourceOwners *g_owners = nullptr;
size_t g_dungeon_count = 0;

bool same(const DungeonState &a, const DungeonState &b) {
    return a.active == b.active && a.pos.dungeon == b.pos.dungeon && a.pos.floor == b.pos.floor &&
           a.pos.x == b.pos.x && a.pos.y == b.pos.y && a.pos.facing == b.pos.facing &&
           !std::memcmp(a.cells, b.cells, sizeof(a.cells)) && !std::memcmp(a.revealed, b.revealed, sizeof(a.revealed)) &&
           a.wanderer.bank == b.wanderer.bank && a.wanderer.type == b.wanderer.type && a.wanderer.x == b.wanderer.x &&
           a.wanderer.y == b.wanderer.y && a.wanderer.floor == b.wanderer.floor && a.wanderer.attr == b.wanderer.attr &&
           a.wanderer.hidden == b.wanderer.hidden && a.wanderer.prev_x == b.wanderer.prev_x &&
           a.wanderer.prev_y == b.wanderer.prev_y && a.quickness_toggle == b.quickness_toggle;
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

// Three recognisable loose objects on the surface beside Deceit: one per
// point in time, so a pool names the generation (or live state) it came from.
QuestObject marker(int n) {
    QuestObject o;
    o.location = 0; o.floor = 0;
    o.x = g_owners->location_x[kDeceit - 1] + n; o.y = g_owners->location_y[kDeceit - 1];
    o.tile = 60 + n; o.loot = true; o.item_id = 3; o.quality = n;
    return o;
}
bool has_marker(const std::vector<QuestObject> &pool, int n) {
    for (const auto &o : pool)
        if (same(o, marker(n))) return true;
    return false;
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
        g.party.character_count = g.party.party_size = 1;                 // staging: one-member party
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

    // The load routes. Every one ends in AlphaSaveService::load/load_slot.
    void menu_save() { raw_key('m', true); ball(RawInputKind::TrackballDown); key('\r'); raw_key('m', true); }
    void menu_load() {                                                    // Load / Save Management -> Continue Latest
        raw_key('m', true); ball(RawInputKind::TrackballDown); ball(RawInputKind::TrackballDown);
        key('\r'); key('\r');
    }
    void menu_load_row(int row) {                                         // Load / Save Management -> Generation <row+1>
        raw_key('m', true); ball(RawInputKind::TrackballDown); ball(RawInputKind::TrackballDown);
        key('\r');
        for (int i = 0; i <= row; ++i) ball(RawInputKind::TrackballDown);
        key('\r');
    }
    void alt_load() { raw_key('l', true); }                                // DeviceShortcut::Load
    void title_continue() {                                               // Return to Title -> Journey Onward -> Continue
        raw_key('m', true); ball(RawInputKind::TrackballUp); key('\r');   // Up wraps to the last root item
        key('x');                                                         // Title -> main menu
        key('j'); key('\r');                                              // Journey Onward -> Continue (latest)
    }
    void close_menu() { raw_key('m', true); }

    void set_mark() { mark = rt->ui()->transcript_size(); }
    bool saw(const char *needle) const {
        for (size_t i = mark; i < rt->ui()->transcript_size(); ++i)
            if (const auto *b = rt->ui()->transcript_at(i); b && std::strstr(b->text, needle)) return true;
        return false;
    }

    bool enter_deceit() {
        g().position.map = {0, 0};
        g().position.xy = {g_owners->location_x[kDeceit - 1], g_owners->location_y[kDeceit - 1]};
        set_quest_flag(g().quest, QuestFlag::Word33);                     // staging: Deceit's Word of Passage
        key('e');
        return d().active && d().pos.dungeon == kDeceit && d().pos.floor == 0 && d().pos.x == 1 && d().pos.y == 1;
    }
    bool fall_to_floor_two() {
        forward(); forward(); turn_left(); forward(); forward(); turn_left(); forward();
        return d().active && d().pos.floor == 2 && d().pos.x == 3 && d().pos.y == 2 &&
               d().pos.facing == DungeonFacing::North;
    }
    bool enter_castle() {
        g().position.map = {0, 0};
        g().position.xy = {g_owners->location_x[kCastle - 1], g_owners->location_y[kCastle - 1]};
        key('e');
        return g().position.map.location == kCastle && g().position.map.floor == 0;
    }
    void stage(int n) { ctx().quest_world->append(ctx().quest_world->context, marker(n)); }  // staging
};

// Everything a player (or the next command) can observe after a load.
struct Snapshot {
    WorldPosition pos{};
    GameTime time{};
    uint32_t gold = 0;
    int karma = 0;
    DungeonState dungeon{};
    std::vector<QuestObject> pool;
    bool ctx_dungeon = false, ctx_combat = false;
    UiMode base_mode = UiMode::Exploration;
    std::vector<int> actors;
    int door_turns = 0;
};
Snapshot snap(Harness &h) {
    Snapshot s;
    s.pos = h.g().position; s.time = h.g().time; s.gold = h.g().gold; s.karma = h.g().karma;
    s.dungeon = h.d(); s.pool = h.pool();
    s.ctx_dungeon = h.rt->command_context().dungeon; s.ctx_combat = h.rt->command_context().combat;
    s.base_mode = h.rt->ui()->base_mode();
    for (size_t i = 0; i < h.rt->actors().count; ++i) {
        const auto &a = h.rt->actors().actors[i];
        s.actors.insert(s.actors.end(), {a.x, a.y, a.z, a.state});
    }
    s.door_turns = int(h.rt->commands().door.turns);
    return s;
}
bool equivalent(const Snapshot &a, const Snapshot &b, const char *tag) {
    bool ok = true;
    auto field = [&](bool eq, const char *name) {
        if (!eq) { std::printf("         %s: differs in %s\n", tag, name); ok = false; }
    };
    field(a.pos.map.location == b.pos.map.location && a.pos.map.floor == b.pos.map.floor && a.pos.xy.x == b.pos.xy.x &&
              a.pos.xy.y == b.pos.xy.y,
          "GameState position");
    field(a.time.year == b.time.year && a.time.month == b.time.month && a.time.day == b.time.day &&
              a.time.hour == b.time.hour && a.time.minute == b.time.minute,
          "clock");
    field(a.gold == b.gold && a.karma == b.karma, "gold/karma");
    field(same(a.dungeon, b.dungeon), "dungeon session");
    field(same(a.pool, b.pool), "loose-object pool");
    field(a.ctx_dungeon == b.ctx_dungeon && a.ctx_combat == b.ctx_combat, "command context");
    field(a.base_mode == b.base_mode, "UI base mode");
    field(a.actors == b.actors, "NPC actors");
    field(a.door_turns == b.door_turns, "open-door tracker");
    return ok;
}

// ---------------------------------------------------------------------------
// The two generations every case starts from.
//   OLD (sequence 1): Deceit floor 0 (1,1) South, gold 111, pool {marker 1}.
//   NEW (sequence 2): Deceit floor 2 (3,2) North, gold 222, pool {1, 2}.
//   live afterwards : turned East, gold 999, pool {1, 2, 3}.
// ---------------------------------------------------------------------------
bool build_two(Harness &h) {
    tdeck::host_memory_save_forget_for_test();
    if (!h.enter_deceit()) return false;
    h.stage(1); h.g().gold = 111;
    h.set_mark(); h.menu_save();
    if (!h.saw("Save complete")) return false;
    if (!h.fall_to_floor_two()) return false;
    h.stage(2); h.g().gold = 222;
    h.set_mark(); h.menu_save();
    if (!h.saw("Save complete")) return false;
    h.turn_right(); h.g().gold = 999; h.stage(3);
    return tdeck::host_memory_save_generations_for_test() == 2;
}
// What each generation restores ON ITS OWN, taken through the production
// Generation row (load_slot) in a runtime of its own. slot = sequence & 1.
constexpr int kOldRow = 1, kNewRow = 0;
Snapshot oracle(int row) {
    Harness o;
    o.menu_load_row(row);
    return snap(o);
}
bool looks_old(const Snapshot &s) {
    return s.gold == 111 && s.dungeon.active && s.dungeon.pos.floor == 0 && has_marker(s.pool, 1) &&
           !has_marker(s.pool, 2) && !has_marker(s.pool, 3);
}
bool looks_new(const Snapshot &s) {
    return s.gold == 222 && s.dungeon.active && s.dungeon.pos.floor == 2 && has_marker(s.pool, 2) &&
           !has_marker(s.pool, 3);
}

// --- sidecar edits ----------------------------------------------------------
using Edit = void (*)(Json &);
void dungeon_level_8(Json &gs) { gs["dungeon"]["floor"] = Json(8); }
void dungeon_cells_511(Json &gs) { gs["dungeon"]["cells"].values.pop_back(); }
void dungeon_facing_4(Json &gs) { gs["dungeon"]["facing"] = Json(4); }
void dungeon_id_0(Json &gs) { gs["dungeon"]["dungeon"] = Json(0); }
void dungeon_id_41(Json &gs) { gs["dungeon"]["dungeon"] = Json(41); }
void dungeon_hidden_number(Json &gs) { gs["dungeon"]["wanderer"]["hidden"] = Json(1); }
void objects_item_9(Json &gs) { gs["worldObjects"].values[0]["item"] = Json(9); }
void objects_entry_number(Json &gs) { gs["worldObjects"].values[0] = Json(7); }
void objects_floor_minus_2(Json &gs) { gs["worldObjects"].values[1]["floor"] = Json(-2); }
void objects_not_array(Json &gs) { gs["worldObjects"] = Json::object(); }

struct Case { const char *id; Edit edit; const char *what; };
const Case kDungeonCases[] = {
    {"C1", dungeon_level_8, "dungeon level 8 (DungeonState has floors 0-7)"},
    {"C2", dungeon_cells_511, "dungeon map of 511 cells (the grid is 8x8x8 = 512)"},
    {"C3", dungeon_facing_4, "dungeon facing 4 (N/E/S/W = 0-3)"},
    {"C4", dungeon_id_0, "dungeon id 0 (dungeons are locations 33-40, 0x21-0x28)"},
    {"C5", dungeon_id_41, "dungeon id 41 (past Doom, 40)"},
    {"C6", dungeon_hidden_number, "wanderer 'hidden' a number, not a bool"},
};
const Case kObjectCases[] = {
    {"D1", objects_item_9, "worldObjects[0].item 9 (PlotItem is 0-8)"},
    {"D2", objects_entry_number, "worldObjects[0] a number, not an object"},
    {"D3", objects_floor_minus_2, "worldObjects[1].floor -2 (floors are -1..255)"},
    {"D4", objects_not_array, "worldObjects an object, not an array"},
};

// ---------------------------------------------------------------------------
// A / B: controls.
// ---------------------------------------------------------------------------
void test_controls(const Snapshot &old_s, const Snapshot &new_s) {
    std::printf("A  control: a valid newest generation loads whole\n");
    {
        Harness h;
        expect(build_two(h), "A0", "precondition: OLD and NEW generations saved, live state moved on");
        h.set_mark(); h.menu_load();
        const auto s = snap(h);
        expect(h.saw("Load complete") && looks_new(s) && equivalent(s, new_s, "A/new"), "A1",
               "Continue Latest restores NEW: gold 222, Deceit floor 2, pool {1,2}, all owners as NEW's own row");
    }
    std::printf("B  control: an unreadable newest generation (CRC) -- already handled before Batch 28\n");
    {
        Harness h;
        build_two(h);
        tdeck::host_memory_save_damage_for_test();
        openu5::FrontendSaveSlot rows[2]{};
        tdeck::AlphaSaveService().inspect(rows);
        expect(rows[kNewRow].present && !rows[kNewRow].valid && rows[kOldRow].valid, "B1",
               "inspect: the torn generation is listed corrupt, the older one valid");
        h.set_mark(); h.menu_load();
        const auto s = snap(h);
        expect(h.saw("Load complete") && looks_old(s) && equivalent(s, old_s, "B/old"), "B2",
               "Continue Latest falls back to OLD, whole");
    }
}

// ---------------------------------------------------------------------------
// C / D / E: newest generation well-formed and CRC-sealed, one sidecar owner
// invalid, the other valid. The whole generation must be refused and OLD
// restored whole -- never NEW's primary state with a dropped session or pool.
// ---------------------------------------------------------------------------
void run_rejection(const Case &c, const Snapshot &old_s, bool dungeon_case) {
    Harness h;
    const bool built = build_two(h);
    const bool edited = tdeck::host_memory_save_edit_for_test(true, c.edit);
    openu5::FrontendSaveSlot rows[2]{};
    tdeck::AlphaSaveService().inspect(rows);
    h.set_mark(); h.menu_load();
    const auto s = snap(h);
    char what[200];
    std::snprintf(what, sizeof(what), "** %s: NEW refused, Continue Latest restores OLD whole **", c.what);
    const bool ok = built && edited && h.saw("Load complete") && looks_old(s) && equivalent(s, old_s, c.id);
    if (!ok)
        std::printf("         got gold=%u session=%d floor=%u pool=%zu (markers %d%d%d)\n", s.gold, s.dungeon.active,
                    unsigned(s.dungeon.pos.floor), s.pool.size(), has_marker(s.pool, 1), has_marker(s.pool, 2),
                    has_marker(s.pool, 3));
    expect(ok, c.id, what);
    char id[16];
    std::snprintf(id, sizeof(id), "%si", c.id);
    expect(rows[kNewRow].present && !rows[kNewRow].valid && rows[kOldRow].valid, id,
           "inspect (the Generation rows): NEW listed corrupt, OLD valid");
    std::snprintf(id, sizeof(id), "%sm", c.id);
    // E: the owner that WAS valid in NEW must not have been adopted either.
    expect(dungeon_case ? !has_marker(s.pool, 2) : s.dungeon.pos.floor != 2, id,
           dungeon_case ? "mixed: NEW's valid worldObjects were not kept beside OLD's state (no marker 2)"
                        : "mixed: NEW's valid dungeon session was not kept beside OLD's state (not floor 2)");
}
void test_rejections(const Snapshot &old_s) {
    std::printf("C  newest generation with an invalid dungeon sidecar (worldObjects valid)\n");
    for (const auto &c : kDungeonCases) run_rejection(c, old_s, true);
    std::printf("D  newest generation with invalid worldObjects (dungeon valid)\n");
    for (const auto &c : kObjectCases) run_rejection(c, old_s, false);
}

// ---------------------------------------------------------------------------
// P: valid but unusual payloads are still accepted, value for value.
// ---------------------------------------------------------------------------
void unusual_dungeon(Json &gs) {
    auto &d = gs["dungeon"];
    for (size_t i = 5 * 64; i < 6 * 64; ++i) d["cells"].values[i] = Json(255);   // floor 5: every cell 0xFF
    for (auto &b : d["revealed"].values) b = Json(255);
    d["wanderer"]["type"] = Json(255); d["wanderer"]["x"] = Json(200); d["wanderer"]["bank"] = Json(255);
    d["dungeon"] = Json(40);                                                        // Doom: edge of the range
}
void unusual_objects(Json &gs) {
    auto &o = gs["worldObjects"].values[1];
    o["tile"] = Json(2047); o["slot"] = Json(255); o["hull"] = Json(255); o["skiffs"] = Json(255);
    o["plotZ"] = Json(-1); o["floor"] = Json(-1); o["item"] = Json(8); o["contents"] = Json(1023);
}
void no_dungeon(Json &gs) { gs.erase("dungeon"); }
void no_objects(Json &gs) { gs.erase("worldObjects"); }
void test_preservation() {
    std::printf("P  valid-but-unusual payloads are accepted as written\n");
    {
        Harness h;
        build_two(h);
        tdeck::host_memory_save_edit_for_test(true, unusual_dungeon);
        h.set_mark(); h.menu_load();
        const auto &d = h.d();
        bool cells = true, revealed = true;
        for (int i = 5 * 64; i < 6 * 64; ++i) cells = cells && d.cells[i] == 0xFF;
        for (auto b : d.revealed) revealed = revealed && b == 0xFF;
        expect(h.saw("Load complete") && h.g().gold == 222 && d.active && d.pos.dungeon == 40 && d.pos.floor == 2 &&
                   cells && revealed && d.wanderer.type == 255 && d.wanderer.x == 200 && d.wanderer.bank == 255,
               "P1", "a floor of 0xFF cells, all-0xFF reveal, a dormant wanderer at x 200, dungeon 40: NEW loads verbatim");
    }
    {
        Harness h;
        build_two(h);
        tdeck::host_memory_save_edit_for_test(true, unusual_objects);
        h.set_mark(); h.menu_load();
        bool found = false;
        for (const auto &o : h.pool())
            found = found || (o.tile == 2047 && o.slot == 255 && o.hull == 255 && o.skiffs == 255 && o.floor == -1 &&
                              o.item == PlotItem::None && o.contents == 1023);
        expect(h.saw("Load complete") && h.g().gold == 222 && found, "P2",
               "an object at tile 2047, slot/hull/skiffs 255, floor -1, contents 1023: NEW loads verbatim");
    }
    {
        Harness h;
        build_two(h);
        tdeck::host_memory_save_edit_for_test(true, no_dungeon);
        h.set_mark(); h.menu_load();
        expect(h.saw("Load complete") && h.g().gold == 222 && !h.d().active && has_marker(h.pool(), 2), "P3",
               "no \"dungeon\" key (a surface or pre-Batch-26 save): NEW loads, no session");
    }
    {
        Harness h;
        build_two(h);
        tdeck::host_memory_save_edit_for_test(true, no_objects);
        h.set_mark(); h.menu_load();
        expect(h.saw("Load complete") && h.g().gold == 222 && h.d().active && h.d().pos.floor == 2 && h.pool().empty(),
               "P4", "no \"worldObjects\" key (a pre-R-14 save): NEW loads, empty pool");
    }
}

// ---------------------------------------------------------------------------
// F: the same refusal on every load route.
// ---------------------------------------------------------------------------
void test_routes(const Snapshot &old_s) {
    std::printf("F  newest refused, older restored, on every route\n");
    struct Route { const char *id; void (Harness::*load)(); const char *name; bool transcript; };
    const Route routes[] = {
        {"F1", &Harness::alt_load, "Alt+L", true},
        {"F2", &Harness::title_continue, "title screen -> Journey Onward -> Continue", false},
    };
    for (const auto &r : routes)
        for (const Case *c : {&kDungeonCases[0], &kObjectCases[0]}) {
            Harness h;
            build_two(h);
            tdeck::host_memory_save_edit_for_test(true, c->edit);
            h.set_mark();
            (h.*r.load)();
            const auto s = snap(h);
            char what[200];
            std::snprintf(what, sizeof(what), "%s, %s: OLD restored whole", r.name, c->what);
            char id[16];
            std::snprintf(id, sizeof(id), "%s%c", r.id, c == &kDungeonCases[0] ? 'd' : 'o');
            expect((!r.transcript || h.saw("Load complete")) && looks_old(s) && equivalent(s, old_s, id), id, what);
        }
    {
        // The Generation row of the refused generation: listed corrupt, so Enter does nothing.
        Harness h;
        build_two(h);
        tdeck::host_memory_save_edit_for_test(true, dungeon_level_8);
        const auto before = snap(h);
        h.set_mark();
        h.menu_load_row(kNewRow);
        expect(!h.saw("Load complete") && equivalent(snap(h), before, "F3"), "F3",
               "System Menu Generation row of the refused generation: nothing loads, nothing changes");
        h.close_menu(); h.close_menu();
    }
}

// ---------------------------------------------------------------------------
// G: no usable generation. Nothing may change, whatever the live context.
// ---------------------------------------------------------------------------
void test_no_valid() {
    std::printf("G  both generations refused: every route reports it and the live world is untouched\n");
    struct Route { const char *id; void (Harness::*load)(); bool transcript; };
    const Route routes[] = {{"G1", &Harness::menu_load, true}, {"G2", &Harness::alt_load, true},
                            {"G3", &Harness::title_continue, false}};
    for (const auto &r : routes) {
        Harness h;
        build_two(h);
        tdeck::host_memory_save_edit_for_test(true, dungeon_level_8);
        tdeck::host_memory_save_edit_for_test(false, objects_item_9);
        const auto before = snap(h);
        h.set_mark();
        (h.*r.load)();
        char what[160];
        std::snprintf(what, sizeof(what),
                      "%s: %sin Deceit (session, pool {1,2,3}, gold 999, East) nothing changed",
                      r.id, r.transcript ? "\"No valid save\"; " : "");
        expect((!r.transcript || h.saw("No valid save")) && equivalent(snap(h), before, r.id) && h.g().gold == 999 &&
                   has_marker(h.pool(), 3),
               r.id, what);
    }
    {
        // Live in Lord British's castle among its NPCs, the two stored generations both refused.
        Harness h;
        build_two(h);
        tdeck::host_memory_save_edit_for_test(true, dungeon_cells_511);
        tdeck::host_memory_save_edit_for_test(false, dungeon_facing_4);
        Harness live;
        const bool castle = live.enter_castle();
        live.g().gold = 777; live.stage(3);
        live.turn_right();
        const auto before = snap(live);
        live.set_mark(); live.menu_load();
        expect(castle && before.actors.size() > 0 && live.saw("No valid save") && equivalent(snap(live), before, "G4"),
               "G4", "live in Lord British's castle with its NPCs, gold 777: nothing changed (actors, position, pool)");
    }
}

// ---------------------------------------------------------------------------
// S: the post-write self-check (alpha_save.cpp "semantic-validation") uses the
// same gate, so a save the loader would refuse is reported as failed and the
// prior generation still loads.
// ---------------------------------------------------------------------------
void test_self_check() {
    std::printf("S  post-write self-check\n");
    Harness h;
    build_two(h);                                                         // NEW = floor 2, gold 222
    QuestObject bad = marker(4);
    bad.item = PlotItem(9);                                               // staging: an object no decoder accepts
    h.ctx().quest_world->append(h.ctx().quest_world->context, bad);
    h.g().gold = 555;
    h.set_mark(); h.menu_save();
    expect(h.saw("Save failed; prior kept") && !h.saw("Save complete"), "S1",
           "a save whose worldObjects the loader would refuse reports \"Save failed; prior kept\"");
    h.set_mark(); h.menu_load();
    const auto s = snap(h);
    expect(h.saw("Load complete") && looks_new(s), "S2",
           "Continue Latest then restores the prior generation (NEW: gold 222, floor 2), not gold 555 with an emptied pool");
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: batch28_save_validation <openu5-alpha1-resources.bin>\n");
        return 2;
    }
    tdeck::AlphaResourcePack pack;
    tdeck::AlphaResourceReport report{};
    if (pack.open(argv[1], report) != ESP_OK) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    static tdeck::AlphaResourceOwners owners{};
    if (pack.load(owners, report) != ESP_OK) { std::fprintf(stderr, "cannot load the resource pack\n"); return 2; }
    g_owners = &owners;
    g_dungeon_count = report.dungeon_count;

    // The oracles: each generation loaded on its own through its Generation row.
    Snapshot old_s, new_s;
    {
        Harness h;
        expect(build_two(h), "O0", "precondition: two generations (OLD seq 1, NEW seq 2)");
        old_s = oracle(kOldRow);
        new_s = oracle(kNewRow);
        expect(looks_old(old_s) && looks_new(new_s), "O1",
               "oracles: OLD's row gives gold 111 / floor 0 / {1}; NEW's row gives gold 222 / floor 2 / {1,2}");
    }
    test_controls(old_s, new_s);
    test_rejections(old_s);
    test_preservation();
    test_routes(old_s);
    test_no_valid();
    test_self_check();

    std::printf("\nbatch28_save_validation: %d/%d checks GREEN, %d RED\n", g_checks - g_failures, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
