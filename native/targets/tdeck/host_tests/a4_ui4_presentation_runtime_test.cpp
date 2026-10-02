// Alpha 4 A4-UI4/PRES1 (targets/tdeck/ALPHA4_UI.md section 8) -- the frontend
// and presentation finalization, read off the REAL AlphaRuntime on the REAL
// tdeck_board.cpp over the fake ST7789 (the A4-UI2 harness).
//
//   F  frontend
//      F1 the main menu's footer names every hotkey the menu takes (S and P too)
//      F2 the boot screens name the firmware's release, not "Alpha 2.0"
//      F3 Small text keeps every glyph's strokes: 5x7 -> 4x6 merges the middle
//         column and the middle row pair, it never drops the last column or row
//      F4 the title and System Menu Settings pages say the same thing; the
//         Developer row has one name
//   H  the gameplay screen
//      H1 a world getdir's status line is "Direction?", not an aim readout
//      H2 the underworld is captioned "Underworld" (HUD caption, save rows)
//      H3 the spyglass's zodiac view is drawn full-square, as the gem view
//      H4 '^' is a glyph (Z-stats marks the readied item with it)
//   E  console feedback (the original's console strings)
//      E1 getdir 0x35EC prints the direction word on the command's own row:
//         "Look-North" (DS 0xa2a6), not "Look-" then "north"
//      E2 a lost battle prints "BATTLE IS LOST!" once (COMBAT 0x0cda)
//      E3 a won battle prints "VICTORY!" once (the latch, COMBAT 0x0cf6); the
//         arena's own "ended" event text is never printed (the reference's
//         combatOut never prints it; D-72 is the same line before the ending)
//      E4 Mix echoes "Mix Reagents" (DS 0xa1b4) and holds 10 ticks after
//         "Mixing..." (CMDS 0x1b88-0x1b9c) before "Done!"
//   S  scene beats
//      S1 the shard ritual's seven bursts: each a 174 ms tile-0 hold with its
//         noise burst (ULTIMA.EXE 0x3522 x7 from CAST 0x16e1-0x16fa)
//      S2 a quake over a still view pulses (every pulse reaches the panel)
//      S3 the Refuge stage: nothing in the darkness until "But thy slumber is
//         disturbed!" places the Avatar (BLCKTHRN 0x09f5), and the last stage
//         is the Avatar alone on black (0x0bc4-0x0be7)
//
//   a4_ui4_presentation_runtime <openu5-alpha1-resources.bin> <openu5-audio.bin> [--dump <dir>]
//
// Built with -DA4_UI4_HEAD_API (tools/a4_ui4_red_first.py) the checks that
// need the batch's new API are compiled out and count as RED.
#include "a4_ui2_harness.h"
#include "../main/location_names.h"
#include "openu5/combat.h"
#include "openu5/debug_developer.h"
#include "openu5/debug_map_picker.h"
#include "openu5/look.h"
#include "openu5/magic.h"
#include "openu5/narrative_scene.h"
#include "openu5/outdoor.h"
#include "openu5/world.h"

using namespace a4_ui2;

namespace {
AudioPackInfo g_audio{};
constexpr uint16_t kBand = 0x0015;

CombatState &cs(Run &h) { return h.rt->combat_state_for_test(); }
CommandContext &ctx(Run &h) { return h.rt->command_context_for_test(); }
bool in_combat(Run &h) { return ctx(h).combat && cs(h).initialized; }
void emit(Run &h, const GameEvent &e) { ctx(h).events.emit(ctx(h).events.context, e); }
void say(Run &h, const char *text) {
    GameEvent e{};
    e.kind = GameEventKind::Message;
    e.text = text;
    emit(h, e);
    h.render(true); // an emitted event marks no frame dirty: draw it
    h.run(20);
}
std::vector<std::string> blocks(Run &h) {
    std::vector<std::string> out;
    for (size_t i = 0; i < h.rt->ui()->transcript_size(); ++i)
        if (const auto *b = h.rt->ui()->transcript_at(i)) out.emplace_back(b->text);
    return out;
}
size_t count_blocks(Run &h, const std::string &text) {
    size_t k = 0;
    for (const auto &b : blocks(h)) k += b == text;
    return k;
}
bool has_block(Run &h, const std::string &text) { return count_blocks(h, text) > 0; }

// --- the oracle: the 5x7 face's own bytes for the few glyphs drawn here --------
// (column bytes, bit 0 = the top row), held by the test, not read from production.
struct Glyph5 { char c; uint8_t cols[5]; };
constexpr Glyph5 kOracle[] = {
    {'d', {0x38, 0x44, 0x44, 0x48, 0x7f}}, {'o', {0x38, 0x44, 0x44, 0x44, 0x38}},
    {'m', {0x7c, 0x04, 0x18, 0x04, 0x78}}, {'^', {0x04, 0x02, 0x01, 0x02, 0x04}},
};
const uint8_t *oracle(char c) {
    for (const auto &g : kOracle)
        if (g.c == c) return g.cols;
    return nullptr;
}
/** Medium (5x7 in 6x8): the glyph 1:1. */
bool medium_bit(char c, int col, int row) {
    const auto *g = oracle(c);
    return g && col < 5 && row < 7 && (g[col] >> row) & 1;
}
/** Small (4x6 in 5x7): column 2+3 and row 3+4 merged (OR), nothing dropped. */
bool small_bit(char c, int col, int row) {
    static const int cols[4][2] = {{0, 0}, {1, 1}, {2, 3}, {4, 4}};
    static const int rows[6][2] = {{0, 0}, {1, 1}, {2, 2}, {3, 4}, {5, 5}, {6, 6}};
    const auto *g = oracle(c);
    if (!g || col >= 4 || row >= 6) return false;
    for (int x = cols[col][0]; x <= cols[col][1]; ++x)
        for (int y = rows[row][0]; y <= rows[row][1]; ++y)
            if ((g[x] >> y) & 1) return true;
    return false;
}
/** Pixels of a transcript cell that differ from the oracle (white on black). */
int cell_mismatch(int x0, int y0, char c, int cell_w, int glyph_w, int glyph_h, bool small) {
    int bad = 0;
    for (int y = 0; y < glyph_h; ++y)
        for (int x = 0; x < cell_w; ++x) {
            const bool ink = x < glyph_w && (small ? small_bit(c, x, y) : medium_bit(c, x, y));
            bad += px(x0 + x, y0 + y) != (ink ? kWhite : kBlack);
        }
    return bad;
}
/** The transcript's on-screen row holding `text`, by the Board's own geometry; -1 if none. */
int transcript_row(Run &h, const std::string &text, uint8_t ui_size) {
    size_t cols = 0, rows = 0;
    tdeck::world_transcript_geometry(ui_size, false, cols, rows);
    UiRenderedLine lines[openu5::kHudTranscriptLines]{};
    const size_t n = h.rt->ui()->visible_lines(lines, rows, cols);
    // The console layout anchors the rows at the bottom (section 8.20).
    const size_t top = h.rt->ui()->console_layout() && n < rows ? rows - n : 0;
    for (size_t i = 0; i < n; ++i)
        if (text == lines[i].text) return int(top + i);
    return -1;
}

void set_text_size(Run &h, int size) {
    h.key('m', true);
    h.down(); h.down(); h.down(); h.key('\r'); // Settings
    h.down(); h.down(); h.down();              // Text / UI
    for (int i = 0; i < 3 && h.rt->device_settings().ui_size != size && h.rt->system_menu_open(); ++i) {
        h.ball(RawInputKind::TrackballRight);
        if (std::string(h.rt->system_menu_view().lines[3]).find(size == 0 ? "Small" : size == 1 ? "Medium" : "Large") != std::string::npos) break;
    }
    h.mic(); h.mic(); // persist, close
    h.run(100);
}

// --- the arena (A3-HF3 / A4-UI2's troll fight) ---------------------------------
int troll_tile(CommandContext &c) {
    for (size_t i = 0; i < c.outdoor->resources->enemy_count; ++i)
        if (const auto *d = c.outdoor->resources->enemies[i]; d && d->index == 41) return d->tile;
    return -1;
}
bool inland_grass(int &gx, int &gy) {
    const auto m = get_active_map(pack->world, MapId{LocationId(0), 0});
    if (m.error != Error::None) return false;
    for (int y = 60; y < 200; ++y)
        for (int x = 60; x < 200; ++x) {
            bool ok = true;
            for (int dy = -1; dy <= 1 && ok; ++dy)
                for (int dx = -2; dx <= 2 && ok; ++dx) ok = m.value.tile_at(x + dx, y + dy) == 5;
            for (int dy = -6; dy <= 6 && ok; ++dy)
                for (int dx = -6; dx <= 6 && ok; ++dx)
                    ok = tile_animation_kind(m.value.tile_at(x + dx, y + dy)) == TileAnimationKind::Static;
            if (ok) { gx = x; gy = y; return true; }
        }
    return false;
}
bool to_grass(Run &h) {
    int gx = -1, gy = -1;
    if (!inland_grass(gx, gy)) return false;
    DebugTeleportRequest r{};
    r.kind = DebugDestinationKind::Britannia;
    r.x = gx;
    r.y = gy;
    if (apply_debug_teleport(ctx(h), r).status != DebugTeleportStatus::Applied) return false;
    h.render(true);
    return true;
}
CombatActor *player_turn(Run &h) {
    auto &s = cs(h);
    if (!in_combat(h) || s.current < 0 || s.current >= s.count) return nullptr;
    auto &a = s.actors[s.current];
    if (a.enemy || a.member == 255 || a.status != CombatStatus::Active) return nullptr;
    return h.rt->ui()->mode() == UiMode::Combat ? &a : nullptr;
}
CombatActor *member_actor(Run &h, int member) {
    auto &s = cs(h);
    for (int i = 0; i < s.count; ++i)
        if (s.actors[i].member == member) return &s.actors[i];
    return nullptr;
}
CombatActor *lone_enemy(Run &h) {
    auto &s = cs(h);
    CombatActor *e = nullptr;
    for (int i = 0; i < s.count; ++i) {
        auto &a = s.actors[i];
        if (!a.enemy || a.status != CombatStatus::Active) continue;
        if (!e) e = &a;
        else a.status = CombatStatus::Fled;
    }
    return e;
}
bool enter_arena(Run &h) {
    if (!to_grass(h)) return false;
    auto &c = ctx(h);
    c.outdoor->enemies.clear();
    OutdoorEnemy troll{};
    troll.definition = 41;
    troll.tile = troll_tile(c);
    troll.x = h.rt->game().position.xy.x + 1;
    troll.y = h.rt->game().position.xy.y;
    c.outdoor->enemies.push_back(troll);
    h.key(' ');
    h.run(1500);
    for (int t = 0; t < 20000 && in_combat(h) && !player_turn(h); t += 5) h.run(5);
    return in_combat(h) && player_turn(h);
}

// ===========================================================================
void test_frontend() {
    std::printf("F  frontend\n");
    {
        Run h({{"Kojac", 'G', 120}});
        h.return_to_title();
        h.run(200);
        h.key(' ');
        h.run(100);
        const auto v = h.rt->frontend_view();
        const std::string footer = v.footer ? v.footer : "";
        check(h.rt->frontend_state() == FrontendState::MainMenu && footer == "Select: arrows / Enter / J C T U A R S P",
              "F1 the main menu's footer names every hotkey it takes (\"" + footer + "\")");
        h.key('s');
        h.run(100);
        const bool settings = h.rt->frontend_state() == FrontendState::Settings;
        const auto sv = h.rt->frontend_view();
        const std::string title_footer = sv.footer ? sv.footer : "";
        h.mic();
        h.run(100);
        h.key('p');
        h.run(200);
        check(settings && h.rt->frontend_state() == FrontendState::PcTransfer,
              "F1b S opens Settings and P opens PC Save Transfer: both letters the footer now names work");
        // F4: the System Menu's Settings page from a game.
        Run g({{"Kojac", 'G', 120}});
        g.key('m', true);
        g.down(); g.down(); g.down(); g.key('\r');
        const auto mv = g.rt->system_menu_view();
        const std::string menu_footer = mv.footer ? mv.footer : "";
        std::string dev_row;
        for (size_t i = 0; i < mv.line_count; ++i)
            if (std::string(mv.lines[i]).rfind("Developer", 0) == 0) dev_row = mv.lines[i];
        check(title_footer == "Left/right changes; Mic saves" && menu_footer == title_footer,
              "F4 both Settings pages say \"Left/right changes; Mic saves\" (title \"" + title_footer + "\", menu \"" + menu_footer + "\")");
        // Changed on purpose by Alpha 4 A4-ENH1 (ALPHA4_UI.md section 10): there is no
        // Developer row to name any more -- not "Developer: Hidden/Visible" in Settings,
        // not "Developer" on the root (Alt+D opens the tools). F4b used to check that the
        // two menus used the same word for it.
        g.mic();
        g.run(50);
        const auto root = g.rt->system_menu_view();
        std::string root_dev;
        for (size_t i = 0; i < root.line_count; ++i)
            if (std::string(root.lines[i]).rfind("Developer", 0) == 0) root_dev = root.lines[i];
        check(root_dev.empty() && dev_row.empty() && mv.line_count == 6,
              "F4b no Developer row in the System Menu: none on the root, none in its six Settings rows (\"" +
                  root_dev + "\", \"" + dev_row + "\")");
        g.mic();
    }
#ifndef A4_UI4_HEAD_API
    {
        check(std::string(tdeck::firmware_release_label("4.0.0-alpha4-ui4-debug")) == "Alpha 4" &&
                  std::string(tdeck::firmware_release_label("3.0.0-alpha3-rc1-debug")) == "Alpha 3" &&
                  std::string(tdeck::firmware_release_label("x")) == "OpenU5",
              "F2 the release label is the firmware version's own major: Alpha 4 / Alpha 3 (OpenU5 when unreadable)");
        auto boot = [](const char *label) {
            bus::install();
            bus::model() = bus::Model{};
            bus::model().timed = false;
            tdeck::Board b{};
            b.set_release_label(label);
            b.initialize_display();
            std::vector<uint16_t> s(bus::gram(), bus::gram() + 320 * 240);
            return s;
        };
        const auto a4 = boot("Alpha 4"), a2 = boot("Alpha 2.0");
        bool outside = false, inside = false;
        for (int y = 0; y < 240; ++y)
            for (int x = 0; x < 320; ++x) {
                const bool differs = a4[size_t(y * 320 + x)] != a2[size_t(y * 320 + x)];
                (y >= 88 && y < 104 ? inside : outside) |= differs;
            }
        check(inside && !outside, "F2b the boot splash draws the label it is given on its version line, and nothing else changes");
    }
#else
    check(false, "F2 the release label (new API: firmware_release_label / Board::set_release_label)");
    check(false, "F2b the boot splash draws the label it is given (new API)");
#endif
    {
        Run h({{"Kojac", 'G', 120}});
        set_text_size(h, 0);
        say(h, "domo");
        const int row = transcript_row(h, "domo", 0);
        int bad = 0;
        for (int i = 0; i < 4 && row >= 0; ++i) bad += cell_mismatch(184 + i * 5, 88 + row * 7, "domo"[i], 5, 4, 6, true);
        check(h.rt->device_settings().ui_size == 0 && row >= 0 && bad == 0,
              "F3 Small text: \"domo\" drawn with every stroke kept (5x7 -> 4x6, the middle column/row merged); " + n(bad) +
                  " pixels off");
        dump("f3-small");
        set_text_size(h, 1);
        say(h, "domo");
        const int r1 = transcript_row(h, "domo", 1);
        int bad1 = 0;
        for (int i = 0; i < 4 && r1 >= 0; ++i) bad1 += cell_mismatch(184 + i * 6, 88 + r1 * 8, "domo"[i], 6, 5, 7, false);
        check(r1 >= 0 && bad1 == 0, "F3b control: Medium is the 5x7 face 1:1, unchanged (" + n(bad1) + " pixels off)");
    }
}

void test_hud() {
    std::printf("H  the gameplay screen\n");
    {
        Run h({{"Kojac", 'G', 120}});
        h.key('l');
        const std::string status = h.rt->status_overlay();
        check(h.rt->ui()->mode() == UiMode::TargetSelection && status == "Direction?",
              "H1 Look- waits for a direction and the status line says \"Direction?\" (was the aim readout; it reads \"" + status + "\")");
        h.mic();
        h.key('a');
        check(std::string(h.rt->status_overlay()) == "Direction?", "H1b Attack- in the world: the same");
        h.mic();
        h.run(50);
    }
    {
        check(std::string(tdeck::hud_location_caption(0, 255, false, 0)) == "Underworld" &&
                  std::string(tdeck::hud_location_caption(0, -1, false, 0)) == "Underworld" &&
                  std::string(tdeck::hud_location_caption(0, 0, false, 0)) == "Britannia",
              "H2 the caption of the outdoor underworld (floor 255, as Native keeps it) is \"Underworld\"");
        Run h({{"Kojac", 'G', 120}});
        h.rt->game().party.character_count = 16;
        DebugTeleportRequest r{};
        r.kind = DebugDestinationKind::Underworld;
        r.floor = 255;
        r.standard_entry = true;
        const auto status = apply_debug_teleport(ctx(h), r).status;
        const bool moved = status == DebugTeleportStatus::Applied;
        h.render(true);
        h.run(50);
        const bool under = moved && h.rt->game().position.map.location == 0 && h.rt->game().position.map.floor == 255;
        h.key('m', true);
        h.down(); h.key('\r'); // Save Game
        h.key('\r');           // Slot 1, empty: saves at once and closes the menu
        h.run(50);
        h.key('m', true);
        h.down(); h.key('\r');
        const auto v = h.rt->system_menu_view();
        const std::string place = v.details[0] ? v.details[0] : "";
        check(under && place == "        Underworld",
              "H2b a save made in the underworld lists its place as Underworld (\"" + place + "\"; teleport " +
                  n(int(status)) + ", floor " + n(h.rt->game().position.map.floor) + ")");
        h.mic(); h.mic();
    }
    {
        Run h({{"Kojac", 'G', 120}}, false, nullptr, true);
        h.run(50);
        static ZodiacView sky{};
        GameEvent z{};
        z.kind = GameEventKind::Zodiac;
        z.zodiac = &sky;
        emit(h, z);
        h.run(50);
        const uint16_t *vp = h.rt->composed_viewport();
        int band = 0, off = 0;
        for (int y = 0; y < 9; ++y)
            for (int x = 0; x < 176; ++x) {
                band += px(4 + x, 4 + y) == kBand;
                off += px(4 + x, 4 + y) != vp[y * 176 + x];
            }
        for (int y = 167; y < 176; ++y)
            for (int x = 0; x < 176; ++x) off += px(4 + x, 4 + y) != vp[y * 176 + x];
        dump("h3-zodiac");
        check(h.rt->transient_probe_for_test().zodiac_view && band == 0 && off == 0,
              "H3 the zodiac view fills the whole 176x176 square: the strip rows are its own pixels (" + n(off) +
                  " off, " + n(band) + " band)");
    }
    {
        Run h({{"Kojac", 'G', 120}});
        say(h, "^");
        const int row = transcript_row(h, "^", 1);
        const int bad = row >= 0 ? cell_mismatch(184, 88 + row * 8, '^', 6, 5, 7, false) : -1;
        check(bad == 0, "H4 '^' is drawn as a caret, not the '?' of an unknown byte (" + n(bad) + " pixels off)");
    }
}

void test_echo() {
    std::printf("E  console feedback\n");
    {
        Run h({{"Kojac", 'G', 120}});
        h.key('l');
        h.up();
        h.run(50);
        const auto b = blocks(h);
        check(has_block(h, "Look-North") && !has_block(h, "north") && !has_block(h, "Look-"),
              "E1 (L)ook then north: one row \"Look-North\" (getdir 0x35EC, DS 0xa2a6)");
        h.key('o');
        h.ball(RawInputKind::TrackballLeft);
        h.run(50);
        check(has_block(h, "Open-West"), "E1b (O)pen then west: \"Open-West\"");
        // Klimb's own direction question (the core's NeedsDirection "klimb")
        // continues the key's "Klimb-" row: it adds no echo of its token.
        const size_t rows0 = h.rt->ui()->transcript_size();
        GameEvent need{};
        need.kind = GameEventKind::NeedsDirection;
        need.text = "klimb";
        emit(h, need);
        h.run(20);
        const bool asked = h.rt->ui()->mode() == UiMode::TargetSelection && std::string(h.rt->status_overlay()) == "Direction?";
        check(asked && h.rt->ui()->transcript_size() == rows0 && !has_block(h, "klimb"),
              "E1c Klimb's direction question adds no \"klimb\" row (the key's \"Klimb-\", DS 0xa1a0, is the row)");
        h.mic();
        h.run(20);
        // The dispatcher's own echoes, verbatim (DATA.OVL DS 0xa142 "Cast...\n",
        // 0xa1f0 "Ready...\n\n", 0xa24c "Use item\n\n", 0xa28c "Z-stats...\n").
        auto echo_of = [&](char k, std::vector<std::string> want) {
            const size_t at = h.rt->ui()->transcript_size();
            h.key(uint8_t(k));
            const auto b = blocks(h);
            bool ok = b.size() >= at + want.size();
            for (size_t i = 0; ok && i < want.size(); ++i) ok = b[at + i] == want[i];
            h.mic();
            h.mic();
            h.run(50);
            return ok;
        };
        const bool cast = echo_of('c', {"Cast..."}), ready = echo_of('r', {"Ready...", ""}),
                   use = echo_of('u', {"Use item", ""}), ztats = echo_of('z', {"Z-stats..."});
        check(cast && ready && use && ztats,
              "E5 the dispatcher's echoes as DATA.OVL has them: Cast... / Ready... and a blank row / Use item and a blank row / Z-stats...");
    }
    {
        Run h({{"Avatar", 'G', 100}, {"Iolo", 'G', 100}, {"Shamino", 'G', 100}});
        bool arena = enter_arena(h);
        auto *e = lone_enemy(h);
        if (e) {
            e->speed = e->strength = 30;
            e->defense = 0;
            e->attack = 50;
            e->hp = e->max_hp = 200;
        }
        for (int m = 0; m < 3; ++m)
            if (auto *a = member_actor(h, m)) {
                a->speed = 0;
                a->defense = 0;
                if (m != 1) {
                    a->status = CombatStatus::Dead;
                    a->hp = 0;
                    h.rt->game().party.characters[m].status = 'D';
                    h.rt->game().party.characters[m].current_hp = 0;
                } else {
                    a->hp = 3;
                    a->position = {5, 5};
                }
            }
        if (e) e->position = {6, 5};
        h.rt->game().party.characters[1].current_hp = 3;
        h.render(true);
        for (int t = 0; t < 30000 && in_combat(h); t += 5) {
            if (player_turn(h)) h.key(' ', false, false, 5000);
            else h.run(5);
        }
        h.run(200);
        check(arena && !in_combat(h) && count_blocks(h, "BATTLE IS LOST!") == 1,
              "E2 a lost battle prints \"BATTLE IS LOST!\" once (was " + n(long(count_blocks(h, "BATTLE IS LOST!"))) + ")");
    }
    {
        Run h({{"Avatar", 'G', 100}});
        bool arena = enter_arena(h);
        if (auto *e = lone_enemy(h)) {
            e->status = CombatStatus::Dead;
            e->hp = 0;
        }
        if (player_turn(h)) h.key(' ', false, false, 5000);
        h.run(200);
        const bool latched = cs(h).victory && has_block(h, "VICTORY!");
        // The arena's own Get (COMBAT.OVL DS 0x6e14 "Get-") and getdir's word.
        bool get_echo = false;
        for (int t = 0; t < 5000 && in_combat(h) && !player_turn(h); t += 5) h.run(5);
        if (player_turn(h)) {
            h.key('g');
            h.up();
            h.run(50);
            get_echo = has_block(h, "Get-North");
        }
        check(get_echo, "E3b the arena's (G)et echoes \"Get-\" and the direction on one row: \"Get-North\"");
        for (int t = 0; t < 30000 && in_combat(h); t += 5) {
            if (player_turn(h)) h.up();
            else h.run(5);
        }
        h.run(200);
        check(arena && latched && !in_combat(h) && count_blocks(h, "VICTORY!") == 1,
              "E3 a won battle prints \"VICTORY!\" once, at the latch, not again as the party walks out (was " +
                  n(long(count_blocks(h, "VICTORY!"))) + ")");
    }
    {
        Run h({{"Kojac", 'G', 120}}, true);
        auto &g = h.rt->game();
        const auto *def = spell_definition(SpellId(0));
        for (auto &r : g.reagent_quantities) r = 0;
        for (int i = 0; i < 8; ++i)
            if (def && (def->reagents & (1 << i))) g.reagent_quantities[i] = 5;
        h.key('m');
        h.key('\r'); // spell row 0
        int marked = 0;
        for (int i = 0; i < 8; ++i)
            if (def && (def->reagents & (1 << i))) {
                h.key('\r');
                h.down();
                ++marked;
            }
        h.key('m');
        h.key('1');
        h.key('\r', false, false, 1000);
        const int64_t mixing = Run::now();
        const bool echo = has_block(h, "Mix Reagents");
        const bool started = has_block(h, "Mixing...");
        bool early = false;
        int64_t done = -1;
        for (int t = 0; t < 2000 && done < 0; t += 5) {
            h.run(5);
            if (has_block(h, "Done!")) done = Run::now();
        }
        early = done >= 0 && done - mixing < 545000;
        check(marked > 0 && echo, "E4 (M)ix echoes \"Mix Reagents\" (DS 0xa1b4)");
        check(started && done >= 0 && !early,
              "E4b \"Done!\" follows \"Mixing...\" after the 10-tick wait (" + n((done - mixing) / 1000) + " ms)");
    }
}

void test_scenes() {
    std::printf("S  scene beats\n");
    {
        Run h({{"Kojac", 'G', 120}}, false, &g_audio, true);
        bool grass = to_grass(h);
        h.run(100);
        const uint16_t *vp = h.rt->composed_viewport();
        auto cell = [&](int c, int r) {
            uint64_t k = 1469598103934665603ull;
            for (int y = 0; y < 16; ++y)
                for (int x = 0; x < 16; ++x) k = (k ^ vp[(r * 16 + y) * 176 + c * 16 + x]) * 1099511628211ull;
            return k;
        };
        const uint64_t plain = cell(5, 4);
        GameEvent e{};
        e.kind = GameEventKind::CellExplosion;
        e.cell_fx.dx = 0;
        e.cell_fx.dy = -1;
        e.cell_fx.bursts = 7;
        emit(h, e);
        const int64_t t0 = Run::now();
        const size_t mark = h.audio.calls.size();
        std::vector<int64_t> on_edges;
        bool was_on = false;
        for (int t = 0; t < 2500; t += 5) {
            h.run(5);
            const bool on = cell(5, 4) != plain;
            if (on && !was_on) on_edges.push_back((Run::now() - t0) / 1000);
            was_on = on;
        }
        std::vector<int64_t> hits;
        for (size_t i = mark; i < h.audio.calls.size(); ++i)
            if (h.audio.calls[i].what == "sfx" && h.audio.calls[i].value == int(SfxId::CombatHit))
                hits.push_back((h.audio.calls[i].us - t0) / 1000);
        bool spaced = hits.size() == 7;
        for (size_t i = 1; i < hits.size(); ++i) spaced = spaced && hits[i] - hits[i - 1] >= 225 && hits[i] - hits[i - 1] <= 235;
        bool edges = on_edges.size() == 7;
        for (size_t i = 1; i < on_edges.size(); ++i)
            edges = edges && on_edges[i] - on_edges[i - 1] >= 225 && on_edges[i] - on_edges[i - 1] <= 235;
        check(grass && edges, "S1 seven bursts reach the cell, one every 174 + 55 ms (" + n(long(on_edges.size())) + " seen)");
        check(spaced, "S1b each burst sounds its noise burst (the CombatHit program, 0x3522's 0x223c) as it lands (" +
                          n(long(hits.size())) + " heard)");
    }
    {
        Run h({{"Kojac", 'G', 120}}, false, nullptr, true);
        bool grass = to_grass(h);
        h.run(200);
        auto view_hash = [] {
            uint64_t k = 1469598103934665603ull;
            for (int y = 13; y < 171; ++y)
                for (int x = 4; x < 180; ++x) k = (k ^ px(x, y)) * 1099511628211ull;
            return k;
        };
        GameEvent q{};
        q.kind = GameEventKind::Quake;
        uint64_t last = view_hash();
        emit(h, q);
        int changes = 0;
        for (int t = 0; t < 1100; t += 5) {
            h.run(5);
            const uint64_t now = view_hash();
            changes += now != last;
            last = now;
        }
        check(grass && changes >= 16, "S2 a quake over a still view: every pulse reaches the panel (" + n(changes) +
                                          " panel changes; 8 pulses = 16)");
    }
    {
        Run h({{"Kojac", 'D', 0}}, true);
        h.rt->game().party.characters[0].current_hp = 0;
        h.run(20);
        const uint16_t *vp = h.rt->composed_viewport();
        auto empty = [&](int c, int r) {
            for (int y = 0; y < 16; ++y)
                for (int x = 0; x < 16; ++x)
                    if (vp[(r * 16 + y) * 176 + c * 16 + x] != 0) return false;
            return true;
        };
        const auto &p = h.rt->narrative_pacer();
        h.key(' ');
        bool dark_seen = false, dark_empty = true, slumber_seen = false, slumber_avatar = false;
        bool vertigo_seen = false, vertigo_alone = true;
        for (int t = 0; t < 60000; t += 5) {
            h.run(5);
            const auto tr = h.transcript();
            const bool slumber = tr.find("But thy slumber is disturbed!") != std::string::npos;
            if (p.phase() == RefugePhase::Void && !slumber) {
                dark_seen = true;
                dark_empty = dark_empty && empty(5, 5);
            }
            if (slumber && !slumber_seen && p.phase() == RefugePhase::Void) {
                slumber_seen = true;
                slumber_avatar = !empty(5, 5);
            }
            if (p.phase() == RefugePhase::Vertigo) {
                if (!vertigo_seen) dump("s3-vertigo");
                vertigo_seen = true;
                vertigo_alone = vertigo_alone && !empty(5, 5) && empty(2, 7) && empty(8, 7) && empty(5, 2);
            }
            if (p.awaiting_key()) h.key(' ', false, false, 5000);
            if (!p.active() && vertigo_seen) break;
        }
        check(dark_seen && dark_empty, "S3 after the darkness line the stage is empty: no Avatar (0x098f-0x09ca clear every object)");
        check(slumber_seen && slumber_avatar, "S3b \"But thy slumber is disturbed!\" places the Avatar at the centre (0x09f5-0x0a0a)");
        check(vertigo_seen && vertigo_alone, "S3c the last stage is the Avatar alone on black (0x0bc4-0x0be7): no ghosts, no apparition");
    }
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: a4_ui4_presentation_runtime <pack> <openu5-audio.bin> [--dump <dir>]\n");
        return 2;
    }
    if (!load_pack(argv[1])) {
        std::printf("RED the pack does not load\n");
        return 1;
    }
    g_audio = tdeck::load_audio_pack_info(argv[2]);
    if (g_audio.state != AudioPackState::Valid) {
        std::printf("RED the audio pack is not valid -- run `npm run pack:audio`\n");
        return 1;
    }
    g_dump = arg_after(argc, argv, "--dump");
    test_frontend();
    test_hud();
    test_echo();
    test_scenes();
    std::printf("A4-UI4 presentation runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
