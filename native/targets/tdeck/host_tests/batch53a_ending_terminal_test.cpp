// Batch 53A -- the state after the final Doom absorption, on the device's own
// runtime.
//
// Phase 7E-A on the Batch 53 image: absorption PASS, VICTORY! PASS, the ENDMSG
// ending text PASS, input responsive -- and then the party standing in Doom's
// enclosed final cell with ordinary movement commands still accepted.
//
// The original (re/notes/batch53a-endgame-terminal.md, from the shipped bytes):
// the combat teardown sees the absorption sentinel and calls the overlay-13
// stub (ULTIMA.EXE 0x7c4a) into ENDGAME.OVL endgame_main (0x0648). That
// routine loads its own scene map (MISCMAPS.DAT) and ENDMSG.DAT, paces every
// page on getkey, and both of its branches end in a loop that never returns
// to the dungeon: 0x04f9 after the proclamation and report, 0x0ac9 in the
// stranded branch. endgame_main's only `ret` (0x0aed) follows the call to
// endgame_datestamp (0x0a70), which itself never returns. No world command,
// no movement and no save exists after game-won.
//
// So the Batch 53 result is Outcome B: the ending text is right, but the
// device falls back into the Dungeon mode the original never re-enters. This
// file is in two parts:
//
//   R  the hardware result, reproduced -- facts that hold before and after the
//      fix (the absorption, game-won, ENDMSG, the final cell and its walls);
//   T  the original-backed contract -- RED on Batch 53, GREEN after 53A:
//      the session enters the terminal UiMode::Ending, and from then on no
//      gameplay command is routed; transcript paging and the device's System
//      Menu / Developer / Load remain; Save refuses (no save of an ended game).
//
// Seam: the Batch 11 host fixture over the shipped pack; every action is a
// RawInputEvent through AlphaRuntime::handle(), as a key on the device.
#include "../main/alpha_runtime.h"
#include "../main/keyboard_matrix.h"
#include "../main/tdeck_board.h"

#include "esp_timer.h"
#include "openu5/combat.h"
#include "openu5/debug_developer.h"
#include "openu5/endgame_scene.h"
#include "openu5/dungeon.h"
#include "openu5/quest_state.h"
#include "openu5/quest_world.h"
#include "openu5/save_json.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace tdeck {
bool host_memory_save_edit_for_test(bool newest, void (*edit)(openu5::save::Json &game_state));
int host_memory_save_generations_for_test();
} // namespace tdeck

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

const char *mode_label(UiMode m) {
    static const char *n[] = {"explore", "dungeon", "combat", "dialogue", "shop", "special", "text", "number", "yes/no",
                              "party", "inventory", "equipment", "spell", "target", "debug", "key-wait", "ending"};
    return size_t(m) < sizeof(n) / sizeof(n[0]) ? n[size_t(m)] : "?";
}

// The real AlphaRuntime over the shipped pack (same shape as Batch 53's).
struct Harness {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    size_t mark = 0;
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
    }
    GameState &g() { return rt->game(); }
    CommandContext &ctx() { return rt->command_context_for_test(); }
    UiMode mode() const { return rt->ui()->mode(); }
    const DungeonState &d() const { return rt->dungeon_state(); }

    static void advance(int64_t us) { openu5_host_virtual_clock_us() += us; }
    bool raw_key(uint8_t code, bool alt = false) {
        advance(100000);
        tdeck::RawInputEvent raw{};
        raw.kind = RawInputKind::Keyboard; raw.code = code; raw.modifiers.alt = alt;
        raw.transition = tdeck::KeyTransition::Pressed; raw.timestamp_us = openu5_host_virtual_clock_us();
        return rt->handle(raw);
    }
    bool key(uint8_t code) { return raw_key(code); }
    bool ball(RawInputKind kind, bool shift = false) {
        advance(100000);
        tdeck::RawInputEvent raw{}; raw.kind = kind; raw.modifiers.shift = shift;
        raw.timestamp_us = openu5_host_virtual_clock_us();
        return rt->handle(raw);
    }
    // The Mic key (matrix 0,6): a short press-release is Cancel.
    void mic() {
        for (auto t : {tdeck::KeyTransition::Pressed, tdeck::KeyTransition::Released}) {
            advance(100000);
            tdeck::RawInputEvent raw{};
            raw.kind = RawInputKind::Keyboard; raw.column = tdeck::kMicrophoneKeyColumn; raw.row = tdeck::kMicrophoneKeyRow;
            raw.transition = t; raw.timestamp_us = openu5_host_virtual_clock_us();
            rt->handle(raw);
        }
    }
    void north() { ball(RawInputKind::TrackballUp); }
    void east() { ball(RawInputKind::TrackballRight); }
    void west() { ball(RawInputKind::TrackballLeft); }

    void set_mark() { mark = rt->ui()->transcript_size(); }
    // The newest transcript block's sequence number: grows with every append,
    // even once the ring is full and transcript_size() has saturated.
    uint32_t last_seq() const {
        const size_t n = rt->ui()->transcript_size();
        const auto *b = n ? rt->ui()->transcript_at(n - 1) : nullptr;
        return b ? b->sequence : 0;
    }
    size_t count(const char *needle) const {
        size_t n = 0;
        for (size_t i = mark; i < rt->ui()->transcript_size(); ++i)
            if (const auto *b = rt->ui()->transcript_at(i); b && std::strstr(b->text, needle)) ++n;
        return n;
    }
    bool saw(const char *needle) const { return count(needle) > 0; }
    void dump(const char *tag) const {
        std::printf("         transcript since mark (%s):\n", tag);
        for (size_t i = mark; i < rt->ui()->transcript_size(); ++i)
            if (const auto *b = rt->ui()->transcript_at(i)) std::printf("           | %s\n", b->text);
    }
    void alt_save() { raw_key('s', true); }
    void alt_load() { raw_key('l', true); }
    // System Menu -> Save (second root item), then close the menu.
    // Alpha 4 A4-SAVE2: Save Game opens Slots 1-3 on the journey's slot; Enter
    // saves there, and an occupied slot asks "Overwrite Slot N?" (No first).
    void menu_save() {
        raw_key('m', true); ball(RawInputKind::TrackballDown); key('\r'); key('\r');
        if (std::strncmp(rt->system_menu_view().title, "Overwrite", 9) == 0) { ball(RawInputKind::TrackballDown); key('\r'); }
        if (rt->system_menu_open()) raw_key('m', true);   // A4-UI3: a successful save returns to the game; only a failed one leaves the menu open.
    }
    // System Menu -> Load / Save Management -> Continue Latest.
    void menu_load() { raw_key('m', true); ball(RawInputKind::TrackballDown); ball(RawInputKind::TrackballDown); key('\r'); key('\r'); }
};

bool dev_teleport(Harness &h, uint8_t location, int16_t floor, int x, int y) {
    DebugTeleportRequest r;
    r.kind = DebugDestinationKind::Dungeon; r.location = location; r.floor = floor; r.x = x; r.y = y;
    r.standard_entry = false;
    return apply_debug_teleport(h.ctx(), r).status == DebugTeleportStatus::Applied;
}

// Phase 7E-A exactly: Developer -> Preset: Endgame, Party size 1, Teleport ->
// Doom Level 6 (floor 6), X 4, Y 7, face East, one step into the pit.
bool reach_final_room(Harness &h, bool box) {
    apply_debug_preset(h.ctx(), DebugPreset::Endgame);
    h.g().wooden_box = box;
    h.g().party.character_count = h.g().party.party_size = 1;
    h.g().party.active_character = 0;
    if (!dev_teleport(h, 40, 6, 4, 7)) return false;
    h.rt->dungeon_state_for_test().pos.facing = DungeonFacing::East;
    h.north();
    return h.rt->command_context().combat && h.rt->combat_state().initialized;
}
// North x4 in the arena, one input per enemy beat on the virtual clock, then
// a few idle beats (the absorption finishes the combat on its own).
void walk_to_soul(Harness &h) {
    for (int i = 0; i < 4 && h.rt->command_context().combat; ++i) { Harness::advance(600000); h.north(); }
    for (int i = 0; i < 6; ++i) { Harness::advance(600000); h.rt->handle(tdeck::RawInputEvent{}); }
}
// A4-END1: ENDGAME.OVL is played now, paced on the scene clock. Answer its box
// questions with `answer`, press a key at every other getkey and let the clock
// run, until it reaches its last screen: the scroll (victory) or the stranded
// room's endless wander.
void drive_ending(Harness &h, char answer) {
    tdeck::Board board;
    for (int i = 0; i < 400; ++i) {
        const auto *s = h.rt->endgame_scene();
        if (!s || !s->active() || s->phase() == EndgamePhase::Scroll || s->phase() == EndgamePhase::Stranded) return;
        if (s->wait() == EndgameWait::YesNo) h.key(uint8_t(answer));
        else if (s->wait() == EndgameWait::Key) h.key(' ');
        for (int t = 0; t < 100; ++t) { Harness::advance(5000); h.rt->render(board); }
    }
}

// Is the final cell enclosed? Pure core, on COPIES: step Forward facing each
// of the four ways and see whether the party ever leaves the cell.
bool cell_enclosed(Harness &h, int &exits) {
    exits = 0;
    for (int f = 0; f < 4; ++f) {
        GameState g = h.g(); TurnState t = h.rt->turn(); DungeonState d = h.d();
        d.pos.facing = DungeonFacing(f);
        dungeon_action(g, t, d, DungeonAction::Forward, DungeonSink{});
        if (d.pos.x != h.d().pos.x || d.pos.y != h.d().pos.y || d.pos.floor != h.d().pos.floor) ++exits;
    }
    return exits == 0;
}

// The newest save generation's game-won flag (or -1 without a generation).
int g_newest_won = -1;
int newest_save_won() {
    g_newest_won = -1;
    tdeck::host_memory_save_edit_for_test(true, [](save::Json &gs) {
        g_newest_won = gs["questFlags"].has("game-won") && gs["questFlags"]["game-won"].truth() ? 1 : 0;
    });
    return g_newest_won;
}

// ===========================================================================
// U. The core rule on its own: UiSession, no device, no resync.
// ===========================================================================
struct Spy {
    size_t count = 0;
    static void send(void *p, const UiIntent &) { ++static_cast<Spy *>(p)->count; }
};
UiAction act(UiActionKind k) { UiAction a; a.kind = k; return a; }
void test_session() {
    std::printf("U  UiSession alone: GameWon enters the Ending, resyncs cannot leave it, input dispatches nothing\n");
    Spy spy;
    UiTextBlock blocks[32]{};
    UiSession ui{{blocks, 32}, {&spy, Spy::send}};
    ui.set_base_mode(UiMode::Dungeon);
    GameEvent won{}; won.kind = GameEventKind::GameWon; won.text = "victory";
    ui.consume(won);
    expect(ui.mode() == UiMode::Ending && ui.ending_active(), "U1", "** a GameWon event puts the session in UiMode::Ending **");
    ui.set_base_mode(UiMode::Dungeon);
    ui.set_base_mode(UiMode::Exploration);
    expect(ui.mode() == UiMode::Ending, "U2", "** set_base_mode(Dungeon / Exploration) -- the per-input resync -- cannot leave it **");
    UiAction north = act(UiActionKind::Direction); north.direction = Direction::North;
    UiAction k = act(UiActionKind::Character); k.character = u'k';
    ui.handle_input(north); ui.handle_input(k); ui.handle_input(act(UiActionKind::Confirm));
    ui.handle_input(act(UiActionKind::Cancel));
    expect(spy.count == 0 && ui.mode() == UiMode::Ending && !ui.accepts_direction_input(), "U3",
           "** a direction, a command letter, Confirm and Cancel dispatch nothing **");
    ui.leave_ending(UiMode::Dungeon);
    ui.handle_input(north);
    expect(ui.mode() == UiMode::Dungeon && !ui.ending_active() && spy.count > 0, "U4",
           "leave_ending(Dungeon) -- the owner's load path -- restores ordinary dispatch");
}

// ===========================================================================
// R. The Phase 7E-A hardware result, reproduced.
// ===========================================================================
void test_reproduction() {
    std::printf("R  Phase 7E-A reproduced: Endgame preset, party of 1, Doom L6 (4,7), East into the pit, North x4\n");
    Harness h;
    const bool room = reach_final_room(h, true);
    expect(room && h.mode() == UiMode::Combat, "R1", "the pit drops the party into the final arena (combat map 127)");
    h.set_mark();
    walk_to_soul(h);
    expect(h.saw("is absorbed!"), "R2", "absorption: \"Avatar is absorbed!\"");
    expect(h.saw("VICTORY!"), "R3", "\"VICTORY!\" is printed as the arena closes");
    expect(quest_flag(h.g().quest, QuestFlag::GameWon), "R4", "game-won is set");
    // A4-END1: the ending is played, not dumped; the report is the scroll's.
    drive_ending(h, 'y');
    const auto *scene = h.rt->endgame_scene();
    std::string report;
    for (int c = 0; scene && c < kEndgameScrollCols; ++c) {
        const auto &cell = scene->scroll()[21 * kEndgameScrollCols + c];
        report += (cell.flags & kEndgameCellPrinted) ? char(cell.ch) : ' ';
    }
    expect(h.count("Lord British carefully opens the box...") == 1 && h.count("\"FOLLOW!\" cries Lord British") == 1 &&
               h.count("Report now, thy Quest compleat") == 0 && report.find("Report now, thy Quest compleat in") != std::string::npos,
           "R5", "the ENDMSG ending (record 9, proclamation, report) is shown once; the report is on the scroll (A4-END1)");
    expect(!h.rt->command_context().combat && !h.rt->combat_state().initialized, "R6", "the arena is torn down");
    const auto &p = h.d().pos;
    std::printf("         final position: dungeon=%u floor=%u (%u,%u) facing=%d active=%d\n", unsigned(p.dungeon),
                unsigned(p.floor), unsigned(p.x), unsigned(p.y), int(p.facing), h.d().active);
    expect(h.d().active && p.dungeon == 40 && p.floor == 7 && p.x == 5 && p.y == 7, "R7",
           "the dungeon session is still Doom floor 7 (5,7), the cell the pit dropped the party into");
    int exits = 0;
    const bool closed = cell_enclosed(h, exits);
    expect(closed, "R8", "that cell is enclosed: Forward in all four facings never leaves it (core, on copies)");
    // The device transcript after the ending, each block cut to 40 characters
    // (the ENDMSG records are EA text and are not reproduced in a log).
    std::printf("         the ending as the device transcript holds it (blocks cut to 40 chars):\n");
    for (size_t i = h.mark; i < h.rt->ui()->transcript_size(); ++i)
        if (const auto *b = h.rt->ui()->transcript_at(i))
            std::printf("           | %.40s%s\n", b->text, std::strlen(b->text) > 40 ? "..." : "");
    // What Batch 53 then does with the next key -- printed, not asserted: the
    // expected result is part T's, decided by the original bytes.
    const auto routed = h.rt->routed_command_count();
    const auto facing = h.d().pos.facing;
    h.east();
    std::printf("         OBSERVED after the ending: mode=%s; one trackball-right routed %u gameplay command(s), facing %d -> %d\n",
                mode_label(h.mode()), unsigned(h.rt->routed_command_count() - routed), int(facing), int(h.d().pos.facing));
}

// ===========================================================================
// T. The original-backed terminal contract.
// ===========================================================================
void test_terminal(bool box) {
    const char *tag = box ? "V" : "S";
    std::printf("T%s the terminal ending %s the wooden box\n", tag, box ? "with" : "without");
    Harness h;
    // A save from BEFORE the ending, for the Load / no-overwrite checks.
    if (!reach_final_room(h, box)) { expect(false, box ? "TV0" : "TS0", "precondition: the final arena opens"); return; }
    h.alt_save();
    const int pre_won = newest_save_won();
    h.set_mark();
    walk_to_soul(h);
    drive_ending(h, box ? 'y' : 'n'); // A4-END1: to the ending's last screen
    char id[8];
    auto I = [&](int n) { std::snprintf(id, sizeof(id), "T%s%d", tag, n); return id; };
    expect(pre_won == 0 && quest_flag(h.g().quest, QuestFlag::GameWon) &&
               (box ? h.saw("FOLLOW!") : h.saw("pull up a chair")), I(0),
           "precondition: a pre-ending save exists; the ending ran and set game-won");
    expect(h.mode() == UiMode::Ending && h.rt->ui()->ending_active(), I(1),
           "** the session is in the terminal UiMode::Ending, not back in Dungeon (ENDGAME.OVL never returns) **");

    // Movement and every ordinary key route nothing.
    const auto routed = h.rt->routed_command_count();
    const auto pos = h.d().pos;
    const auto time = h.g().time;
    const uint32_t lines = h.last_seq();
    h.north(); h.east(); h.west();
    expect(h.rt->routed_command_count() == routed && h.d().pos.facing == pos.facing && h.d().pos.x == pos.x &&
               h.d().pos.y == pos.y && h.mode() == UiMode::Ending, I(2),
           "** trackball forward / turn right / turn left route no gameplay command; facing and cell unchanged **");
    for (uint8_t k : {uint8_t('\r'), uint8_t(' '), uint8_t('k'), uint8_t('z'), uint8_t('c'), uint8_t('w'), uint8_t('\b')}) h.key(k);
    h.mic();
    expect(h.rt->routed_command_count() == routed && h.mode() == UiMode::Ending &&
               h.g().time.hour == time.hour && h.g().time.minute == time.minute, I(3),
           "** Enter, Space, K, Z, C, W, Backspace and Mic (Cancel) route nothing; no turn passes **");
    expect(h.last_seq() == lines, I(4),
           "swallowed keys add nothing to the transcript (no per-key noise, no \"Blocked!\")");

    // Transcript paging still reads the ending.
    h.ball(RawInputKind::TrackballUp, true);
    const size_t scrolled = h.rt->ui()->scroll_offset_lines();
    h.ball(RawInputKind::TrackballDown, true);
    expect(scrolled > 0 && h.rt->ui()->scroll_offset_lines() == 0 && h.mode() == UiMode::Ending, I(5),
           "Shift+Up / Shift+Down page the ending text and return to it");

    // No save of an ended game.
    h.set_mark();
    h.alt_save();
    const int after_alt_s = newest_save_won();
    expect(!h.saw("Save complete") && after_alt_s == 0, I(6),
           "** Alt+S writes nothing: the newest save is still the pre-ending one **");
    h.menu_save();
    expect(!h.saw("Save complete") && newest_save_won() == 0 && !h.rt->system_menu_open(), I(7),
           "** System Menu -> Save writes nothing either **");
    expect(h.count("Save unavailable") >= 2, I(8), "both refusals say so on the System line");

    // The device's own menus still answer, and hand back to the Ending.
    h.raw_key('m', true);
    const bool menu = h.rt->system_menu_open();
    h.raw_key('m', true);
    expect(menu && !h.rt->system_menu_open() && h.mode() == UiMode::Ending, I(9),
           "Alt+M opens and closes the System Menu over the ended game");
    h.raw_key('d', true);
    const bool dev = h.mode() == UiMode::DebugMenu;
    h.mic();
    expect(dev && h.mode() == UiMode::Ending, I(10), "Alt+D opens Developer; leaving it returns to the Ending");
    expect(quest_flag(h.g().quest, QuestFlag::GameWon) && h.count("THE QUEST OF THE AVATAR IS FOREVER") == 0 &&
               h.count("pull up a chair") == 0, I(11),
           "game-won survives all of it and the ending never re-runs");

    // Load leaves the Ending: the pre-ending save resumes ordinary play.
    h.set_mark();
    h.alt_load();
    const auto routed_before = h.rt->routed_command_count();
    const bool loaded = h.saw("Load complete") && !quest_flag(h.g().quest, QuestFlag::GameWon);
    h.east();
    expect(loaded && h.mode() != UiMode::Ending && h.rt->routed_command_count() > routed_before, I(12),
           "** Alt+L restores the pre-ending save and leaves the Ending: the next key is a gameplay command again **");
}

// A Developer change that un-wins the game (Preset: Endgame clears game-won)
// leaves the Ending, so Test A can be rerun without a reload; and a save of an
// already-won game (Batch 53 allowed one) loads INTO the Ending, never back
// into play.
void test_invariant() {
    std::printf("TI the Ending follows game-won: Developer un-win, and a legacy won save\n");
    Harness h;
    if (!reach_final_room(h, true)) { expect(false, "TI0", "precondition: the final arena opens"); return; }
    h.alt_save();
    walk_to_soul(h);
    expect(h.mode() == UiMode::Ending, "TI1", "precondition: the Ending is live");
    // Developer -> Shortcuts -> Preset: Endgame is what the checklist uses; it
    // clears game-won (debug_developer.cpp). The device resynchronizes after
    // every input, so the next input leaves the Ending.
    h.raw_key('d', true);
    apply_debug_preset(h.ctx(), DebugPreset::Endgame);
    h.mic();
    expect(!quest_flag(h.g().quest, QuestFlag::GameWon) && h.mode() == UiMode::Dungeon, "TI2",
           "Developer Preset: Endgame (game-won cleared) leaves the Ending for the dungeon");
    // The first run left Doom room 15 entered: its cleared bit is set and the
    // live session's cell is 0xa?, so the pit no longer opens the arena. The
    // rerun route therefore clears the rooms (Preset: Dungeon) and re-enters
    // Doom from the surface so the floor is re-read (dungeon_load).
    const bool cleared = dungeon_room_cleared(h.g(), 40, 15);
    std::printf("         after the first ending: Doom room 15 cleared=%d, cell (5,7) floor 7 = 0x%02x\n", cleared,
                unsigned(dungeon_cell(h.d(), 7, 5, 7)));
    apply_debug_preset(h.ctx(), DebugPreset::Dungeon);
    DebugTeleportRequest out;
    out.kind = DebugDestinationKind::Britannia; out.x = 90; out.y = 100; out.standard_entry = false;
    const bool surfaced = apply_debug_teleport(h.ctx(), out).status == DebugTeleportStatus::Applied;
    h.mic();                                                         // any input: the device resyncs
    const bool again = surfaced && reach_final_room(h, true);
    walk_to_soul(h);
    expect(again && quest_flag(h.g().quest, QuestFlag::GameWon) && h.mode() == UiMode::Ending, "TI3",
           "Test A reruns without a reload: Preset: Dungeon, a surface teleport, then the 7E-A route again");
    // A legacy won save (Batch 53 allowed Alt+S after the ending), loaded from
    // ORDINARY play: un-win first, so only the load itself can enter the Ending.
    h.raw_key('d', true);
    apply_debug_preset(h.ctx(), DebugPreset::Endgame);
    h.mic();
    const bool playing = h.mode() != UiMode::Ending && !quest_flag(h.g().quest, QuestFlag::GameWon);
    tdeck::host_memory_save_edit_for_test(true, [](save::Json &gs) { gs["questFlags"]["game-won"] = save::Json(true); });
    h.set_mark();
    h.alt_load();
    const auto routed = h.rt->routed_command_count();
    h.north();
    expect(playing && h.saw("Load complete") && quest_flag(h.g().quest, QuestFlag::GameWon) &&
               h.mode() == UiMode::Ending && h.rt->routed_command_count() == routed, "TI4",
           "** from ordinary play, a save of an already-won game loads into the Ending, never back into play **");
    expect(h.count("The quest is complete.") == 1, "TI5", "and says why once (\"The quest is complete.\")");
    // The System Menu load returns before the per-input resync, so only the
    // load path itself can leave the Ending there: checked BEFORE any key.
    tdeck::host_memory_save_edit_for_test(true, [](save::Json &gs) { gs["questFlags"]["game-won"] = save::Json(false); });
    h.set_mark();
    h.menu_load();
    const bool left = h.saw("Load complete") && !h.rt->system_menu_open() && h.mode() != UiMode::Ending;
    const auto before = h.rt->routed_command_count();
    h.east();
    expect(left && h.rt->routed_command_count() > before, "TI6",
           "** System Menu -> Continue Latest (a not-won save) leaves the Ending at once: the first key plays **");
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: batch53a_ending_terminal_test <openu5-alpha1-resources.bin>\n");
        return 2;
    }
    openu5_host_virtual_clock_us() = 1'000'000;
    tdeck::AlphaResourcePack pack;
    if (pack.open(argv[1], g_report) != ESP_OK) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    static tdeck::AlphaResourceOwners owners{};
    if (pack.load(owners, g_report) != ESP_OK) { std::fprintf(stderr, "cannot load the resource pack\n"); return 2; }
    g_owners = &owners;
    for (size_t i = 0; i < owners.combat_map_count; ++i) g_arenas.push_back({owners.combat_map_views[i], owners.combat_sprites + i * 16});

    test_session();
    test_reproduction();
    test_terminal(true);
    test_terminal(false);
    test_invariant();

    std::printf("\nbatch53a_ending_terminal: %d/%d checks GREEN, %d RED\n", g_checks - g_failures, g_checks, g_failures);
    return g_failures ? 1 : 0;
}
