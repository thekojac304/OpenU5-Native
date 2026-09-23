// Batch 26 -- H-115: save and load inside a dungeon, and the loose world
// objects a save carries, against the shipped binaries.
//
// What the 1988 game does (read from the binaries with re/tools/dis16.py):
//
//   * Saving underground is allowed. DUNGEON.OVL:0x06C4 handles only the
//     arrows, '5', 0x0B, Enter/'.', ^S/^V and the digits; every other key,
//     'Q' included, reaches 0x07A0 -> kernel_cmd_dispatch 0x3178. There 'Q'
//     (0x34C0) goes to 0x338C: print "Quit:" (DS 0xA1EA), call thunk 0x81AE ->
//     CAST2.OVL:0x10FE, which asks "Save game?" (0x9658), prints "Saving..."
//     and writes the whole DGROUP window 0x55A6..0x6605 (0x1060 bytes) to
//     SAVED.GAM with one write (0x1185-0x1194), then "Done." (0x96AC). No
//     g_location (DS 0x5893) test anywhere on that path.
//   * That window holds the dungeon: g_location 0x5893 (0x21..0x28),
//     g_floor 0x5895, x/y 0x5896/0x5897, g_dng_facing 0x6603, the whole
//     8-floor map g_dng_map 0x595A (512 B, with every trap sprung, chest
//     opened and field cast), the rooms-cleared bits 0x58E0 and the object
//     table 0x5C5A that holds the dungeon wanderer.
//   * Loading (INTRO.OVL 0x0EB4) reads the window back verbatim; the main
//     loop (ULTIMA.EXE 0x00DB) sees g_location >= 0x21 and re-enters the
//     session at DUNGEON.OVL:0x0E2E with no file read. 0x0E40 zeroes the
//     local [bp-4] that becomes di, the Rel Tym every-other-turn toggle
//     (0x0F15/0x0F25) -- so that one bit is NOT carried across a load.
//
// The port keeps the surface return position in GameState and the live
// session in DungeonState; the save carries the second in the sidecar's
// "dungeon" object (Batch 6, R-15). Before Batch 26 the sidecar exporter's
// key list (persistence.cpp `extras`) did not include "dungeon", so the
// payload never reached disk and every dungeon save loaded at the entrance
// on the surface -- the H-115 hardware report.
#include "../main/alpha_runtime.h"

#include "openu5/dungeon.h"
#include "openu5/gameplay_save.h"
#include "openu5/persistence.h"
#include "openu5/quest_state.h"
#include "openu5/save_json.h"
#include "openu5/world_commands.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

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
    // In a dungeon the trackball is Forward (up) and turn left/right.
    void forward() { ball(RawInputKind::TrackballUp); }
    void turn_left() { ball(RawInputKind::TrackballLeft); }
    void turn_right() { ball(RawInputKind::TrackballRight); }

    // The device's System Menu (Alt+M): Resume, Save, Load / Save Management, ...
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

    // The ordinary way in: stand on the entrance and (E)nter. A dungeon
    // entrance is drawn sealed (tile 223, quest_world_tile) until its Word of
    // Passage is known, so the fixture grants that one word.
    bool enter_deceit() {
        g().position.map = {0, 0};
        g().position.xy = {g_owners->location_x[kDeceit - 1], g_owners->location_y[kDeceit - 1]};
        set_quest_flag(g().quest, QuestFlag::Word33);
        key('e');
        return d().active && d().pos.dungeon == kDeceit && d().pos.floor == 0 && d().pos.x == 1 && d().pos.y == 1;
    }
    // Deceit floor 0 from the (1,1) ladder: S, S, E, E, then N onto the 0x61
    // at (3,2). It is a trap (DUNGEON 0x0A9F clears it to 0x60) and floor 1's
    // (3,2) is another, so the party lands on floor 2 at (3,2) facing North.
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
    bool klimb_down() {                                                   // authored ladder pair (1,1)
        g().position.xy = {1, 1};
        key('k');
        return g().position.map.location == kCastle && g().position.map.floor == kBasement;
    }
    ActionResult run(CommandKind k, Direction dir) {
        Command c; c.kind = k; c.direction = dir; c.has_direction = true;
        return execute_command(ctx(), c);
    }
    void open_chest(const Cell &c) {                                      // from the cell south of it
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

// ---------------------------------------------------------------------------
// C: the storage chain itself -- capture -> .GAM + sidecar -> parse -> restore,
// the same calls AlphaSaveService::save()/load() make (alpha_save.cpp).
// ---------------------------------------------------------------------------
struct Disk { save::Gam gam{}; std::string sidecar; };
bool write_disk(const GameState &g, const TurnState &t, save::Json retained, const DungeonState &d, Disk &out) {
    save::capture_dungeon(d, retained);
    save::Json side;
    if (save::export_native_state(g, t, retained, g_owners->initial_gam, g_owners->initial_gam_size, out.gam, side,
                                  true) != save::Error::None)
        return false;
    return save::encode_json(side, out.sidecar) == save::JsonError::None;
}
save::Error read_disk(const Disk &in, DungeonState &d) {
    GameState g; TurnState t; save::Json doc; save::SidecarSource source;
    auto e = save::load_native_state(in.gam.data(), in.gam.size(), &in.sidecar, g, t, doc, source, true);
    return e == save::Error::None ? save::restore_dungeon(doc, d) : e;
}

void test_storage_chain() {
    std::printf("C  the dungeon session through the device's storage chain\n");
    GameState g; TurnState t; save::Json retained; save::SidecarSource source;
    if (save::load_native_state(g_owners->initial_gam, g_owners->initial_gam_size, nullptr, g, t, retained, source,
                                true) != save::Error::None) {
        expect(false, "C0", "INIT.GAM loads as a document");
        return;
    }
    DungeonState live;
    live.active = true;
    live.pos = {35, 3, 4, 6, DungeonFacing::East};
    for (size_t i = 0; i < 512; ++i) live.cells[i] = uint8_t((i * 7) & 0xff);
    for (size_t i = 0; i < 64; ++i) live.revealed[i] = uint8_t(i * 3);
    live.wanderer = {2, 0x16, 5, 3, 3, 0x60, true, 255, 255};
    live.quickness_toggle = 1;
    Disk disk;
    expect(write_disk(g, t, retained, live, disk), "C0", "export_native_state + encode_json accept a dungeon save");
    const bool on_disk = disk.sidecar.find("\"dungeon\"") != std::string::npos;
    expect(on_disk, "C1", "** the sidecar written to the card carries the \"dungeon\" object (window 0x5893/0x595A/0x6603) **");
    DungeonState back; back.pos.dungeon = 40;                            // a stale session the load must replace
    const auto e = read_disk(disk, back);
    if (!expect(e == save::Error::None && back.active && same(back, live), "C2",
                "** load_native_state + restore_dungeon give back that exact session (pos, facing, 512 cells, reveal, wanderer) **"))
        describe(back, "restored");
    if (!expect(e == save::Error::None && back.active && back.quickness_toggle == 0, "C3",
                "** the Rel Tym toggle is 0 after a load: DUNGEON 0x0E40 zeroes [bp-4] on every session entry **"))
        describe(back, "restored");
    // Compatibility: a sidecar written before Batch 26 has no "dungeon" key
    // (that is precisely what every pre-26 dungeon save looks like on disk).
    DungeonState surface;
    Disk old;
    write_disk(g, t, retained, surface, old);
    DungeonState stale; stale.active = true; stale.pos.dungeon = 40;
    const auto e2 = read_disk(old, stale);
    expect(old.sidecar.find("\"dungeon\"") == std::string::npos && e2 == save::Error::None && !stale.active, "C4",
           "a sidecar without \"dungeon\" (surface save, or any pre-Batch-26 save) loads as no session, no error");
}

// ---------------------------------------------------------------------------
// D: the device routes -- real (E)nter, real trackball, real System Menu.
// ---------------------------------------------------------------------------
DungeonState g_saved_session;
GameState g_saved_game;

void test_dungeon_same_session() {
    std::printf("D1 save on Deceit floor 2, change everything, System Menu load\n");
    Harness h;
    expect(h.enter_deceit(), "D1a", "precondition: (E)nter with the Word of Passage opens Deceit at floor 0 (1,1)");
    expect(h.fall_to_floor_two() && (h.d().cells[0 * 64 + 2 * 8 + 3] == 0x60) && (h.d().cells[1 * 64 + 2 * 8 + 3] == 0x60),
           "D1b", "precondition: the (3,2) traps on floors 0 and 1 fired (0x61 -> 0x60); party on floor 2 (3,2) facing N");
    h.set_mark();
    h.menu_save();
    expect(h.saw("Save complete"), "D1c", "the System Menu Save reports \"Save complete\" underground (CAST2 0x10FE has no location gate)");
    g_saved_session = h.d();
    g_saved_game = h.g();
    const auto minute = h.g().time.minute;
    // Everything the save must be the only source of, changed by play.
    h.turn_right(); h.forward(); h.turn_right(); h.forward(); h.forward();
    const bool changed = !same(h.d(), g_saved_session) && h.g().time.minute != minute;
    h.g().gold = 999;                                                     // a .GAM field, as a load witness
    expect(changed, "D1d", "precondition: after the save the party turned and moved and time passed");
    h.menu_load();
    expect(h.g().gold == 321 && h.g().time.minute == minute, "D1e", "the load really happened: gold 999 -> 321, clock back");
    if (!expect(h.d().active && same(h.d(), g_saved_session), "D1f",
                "** the party is back in Deceit on floor 2 (3,2) facing N with the saved map, reveal and wanderer **")) {
        describe(g_saved_session, "saved");
        describe(h.d(), "loaded");
    }
    expect(h.d().active && h.d().cells[2 * 8 + 3] == 0x60 && h.d().cells[64 + 2 * 8 + 3] == 0x60, "D1g",
           "** the sprung traps stay sprung (0x60): the map comes from the save, not a DUNGEON.DAT re-read **");
    expect(h.g().position.map.location == 0 && h.g().position.xy.x == g_owners->location_x[kDeceit - 1] &&
               h.g().position.xy.y == g_owners->location_y[kDeceit - 1],
           "D1h", "the surface return position is the Deceit entrance, as saved");
    expect(h.d().active && h.ctx().dungeon && h.mode() == UiMode::Dungeon, "D1i",
           "the session is live the moment the load ends: dungeon context and Dungeon UI mode");
}

void test_dungeon_power_cycle() {
    std::printf("D2 power cycle: a fresh runtime on the surface, System Menu Continue Latest\n");
    Harness h;
    h.g().position.xy = {g_owners->location_x[kCastle - 1], g_owners->location_y[kCastle - 1]};
    expect(!h.d().active, "D2a", "precondition: nothing in RAM -- no dungeon session, standing outside the castle");
    h.menu_load();
    if (!expect(h.d().active && same(h.d(), g_saved_session) && h.d().quickness_toggle == 0, "D2b",
                "** the whole Deceit session comes back from storage alone **")) {
        describe(g_saved_session, "saved");
        describe(h.d(), "loaded");
    }
    expect(h.ctx().dungeon && h.mode() == UiMode::Dungeon, "D2c", "Dungeon UI mode the moment the load ends");
    const WorldPosition surface = h.g().position;
    h.turn_right();
    expect(h.d().active && h.d().pos.facing == DungeonFacing::East && h.d().pos.floor == 2 && h.d().pos.x == 3 &&
               h.d().pos.y == 2,
           "D2d", "the very next trackball input turns the restored party (North -> East) where it stands");
    expect(h.g().position.map.location == surface.map.location && h.g().position.xy.x == surface.xy.x &&
               h.g().position.xy.y == surface.xy.y,
           "D2e", "and the surface return position did not change");
}

void test_surface_control() {
    std::printf("D3 surface saves are unaffected, and a surface save made after a dungeon load stays a surface save\n");
    Harness h;
    expect(h.enter_deceit(), "D3a", "precondition: in Deceit floor 0 at the (1,1) up ladder");
    h.menu_save();
    h.menu_load();                                                        // retained_ now holds a "dungeon" object
    expect(h.d().active && h.d().pos.floor == 0 && h.d().pos.x == 1 && h.d().pos.y == 1, "D3b",
           "precondition: saved and reloaded in the dungeon");
    h.key('k');                                                           // Klimb the up ladder out
    const WorldPosition outside = h.g().position;
    expect(!h.d().active && outside.map.location == 0, "D3c", "precondition: (K)limb at (1,1) exits to Britannia");
    h.menu_save();                                                        // a SURFACE save
    h.enter_deceit();
    h.forward();
    expect(h.d().active, "D3d", "precondition: back underground after the surface save");
    h.menu_load();
    if (!expect(!h.d().active && !h.ctx().dungeon && h.mode() != UiMode::Dungeon, "D3e",
                "** loading the surface save ends the dungeon session: no stale \"dungeon\" object rode along **"))
        describe(h.d(), "loaded");
    expect(h.g().position.map.location == 0 && h.g().position.xy.x == outside.xy.x && h.g().position.xy.y == outside.xy.y,
           "D3f", "the party stands where the surface save was made");
}

// ---------------------------------------------------------------------------
// L: loose world objects -- the object register DS 0x5C5A is inside the save
// window, so the current map's objects load verbatim (0x0408(0) on load
// never reaches 0x1694). In the port: AlphaRuntime::objects_.
// ---------------------------------------------------------------------------
std::vector<QuestObject> g_saved_pool;

void test_loose_objects_same_session() {
    std::printf("L1 opened chest and spilled loot in the castle basement, pool changed after the save\n");
    Harness h;
    expect(h.enter_castle() && h.klimb_down(), "L1a", "precondition: Lord British's basement");
    h.open_chest(kChests[0]);
    const size_t piles = h.loot_at(kChests[0]);
    expect(!h.chests_at(kChests[0]) && piles > 0 && h.chests_at(kChests[1]) == 1 && h.chests_at(kChests[2]) == 1, "L1b",
           "precondition: chest (16,21) opened onto the floor; (17,22) and (13,23) closed");
    h.menu_save();
    g_saved_pool = h.pool();
    h.open_chest(kChests[1]);
    h.open_chest(kChests[2]);
    expect(!h.chests_at(kChests[1]) && !h.chests_at(kChests[2]) && h.loot_at(kChests[1]) > 0, "L1c",
           "precondition: after the save the other two chests were opened too");
    h.menu_load();
    if (!expect(same(h.pool(), g_saved_pool), "L1d",
                "** the object pool is exactly the saved one, entry for entry and field for field **")) {
        describe(g_saved_pool, "saved");
        describe(h.pool(), "loaded");
    }
    expect(h.chests_at(kChests[1]) == 1 && h.chests_at(kChests[2]) == 1 && !h.loot_at(kChests[1]) &&
               !h.loot_at(kChests[2]) && h.loot_at(kChests[0]) == piles && !h.chests_at(kChests[0]),
           "L1e", "(17,22) and (13,23) are closed again, once each; (16,21) is open with its loot, not refilled");
}

void test_loose_objects_power_cycle() {
    std::printf("L2 power cycle with a different pool live: nothing from before the load survives it\n");
    Harness h;
    expect(h.enter_castle() && h.klimb_down(), "L2a", "precondition: a fresh runtime in the basement (chests hydrated)");
    h.open_chest(kChests[2]);                                             // pre-load objects the save never had
    expect(!h.chests_at(kChests[2]) && h.loot_at(kChests[2]) > 0, "L2b", "precondition: (13,23) opened before the load");
    h.menu_load();
    if (!expect(same(h.pool(), g_saved_pool), "L2c", "** after the load the pool is the saved one exactly -- from storage **")) {
        describe(g_saved_pool, "saved");
        describe(h.pool(), "loaded");
    }
    expect(!h.loot_at(kChests[2]) && h.chests_at(kChests[2]) == 1 && h.chests_at(kChests[1]) == 1, "L2d",
           "no stale pre-load loot at (13,23), no duplicate chest anywhere");
}

// ---------------------------------------------------------------------------
// Observation only (not a check): H-164, queued. The Alt+L arm does not run
// synchronize_loaded_world(), so it restores GameState but not the dungeon
// session. Printed so the queued row has host evidence; Batch 26 does not
// change that arm.
// ---------------------------------------------------------------------------
void observe_alt_l() {
    std::printf("Q  observation (H-164, queued, not asserted): Alt+L underground\n");
    Harness h;
    h.enter_deceit();
    h.fall_to_floor_two();
    h.menu_save();
    const DungeonState saved = h.d();
    h.turn_right(); h.turn_right();
    h.raw_key('l', true);
    std::printf("  INFO  H-164 Alt+L: dungeon session %s the save (saved facing %d, now %d)\n",
                same(h.d(), saved) ? "matches" : "does NOT match", int(saved.pos.facing), int(h.d().pos.facing));
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: batch26_dungeon_save_test <openu5-alpha1-resources.bin>\n");
        return 2;
    }
    tdeck::AlphaResourcePack pack;
    tdeck::AlphaResourceReport report{};
    if (pack.open(argv[1], report) != ESP_OK) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    static tdeck::AlphaResourceOwners owners{};
    if (pack.load(owners, report) != ESP_OK) { std::fprintf(stderr, "cannot load the resource pack\n"); return 2; }
    g_owners = &owners;
    g_dungeon_count = report.dungeon_count;

    test_storage_chain();
    test_dungeon_same_session();
    test_dungeon_power_cycle();
    test_surface_control();
    test_loose_objects_same_session();
    test_loose_objects_power_cycle();
    observe_alt_l();

    std::printf("\nbatch26_dungeon_save: %d/%d checks GREEN, %d RED\n", g_checks - g_failures, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
