// Alpha 4 A4-UI4 (targets/tdeck/ALPHA4_UI.md section 8.20): the console
// package and the reverse-video lists the user chose (variant B), on the REAL
// AlphaRuntime and the REAL tdeck_board.cpp over the fake ST7789 (the A4-UI2
// harness: bus untimed, the host esp_timer shim's virtual clock).
//
//   C  the console: the echo bullet, getkey's blank row, bottom anchoring, the
//      live prompt row and the flame-wave wait cursor (poll_key_blink_cursor
//      0x1b38, IBM.CH 0x05-0x08 from the pack), at every text size; answers
//      and messages carry no bullet; busy holds show no prompt and no cursor;
//      a scrolled-back view shows no cursor; the layout is the device's alone
//      (a plain UiSession keeps its lines).
//   P  the lists: an in-game picker and a shop select in reverse video
//      (kernel 0x2a28's bar, as every menu since A4-UI1), no green '>'.
//   H  the console's height (A4-UI4 hardware follow-up, section 8.22): a full
//      console fills the black area from the status box's rule (y 88) to the
//      glass -- Small 21 rows, Medium 19, Large 15; above a context bar 18 / 15
//      / 12 -- bottom-anchored, paging by exactly the rows it draws, with the
//      bullet and the wave cursor on its last row and nothing outside it.
//
// Build with -DA4_UI4_HEAD_API to compile against a Board/UiSession without
// the console API (tools/a4_ui4_red_first.py): those reads report "absent".
//
//   a4_ui4_console_runtime <openu5-alpha1-resources.bin> <openu5-audio.bin> [--dump <dir>]
#include "a4_ui2_harness.h"
#include "../main/alpha_audio.h"
#include "openu5/hud.h"
#include "openu5/debug_map_picker.h"
#include "openu5/magic.h"
#include "openu5/presentation.h"
#include "openu5/world.h"

using namespace a4_ui2;

namespace {
AudioPackInfo g_audio{};
constexpr uint16_t kBand = 0x0015, kGreen = 0x07e0;
constexpr int kX = openu5::kHudRightX, kY = openu5::kHudTranscriptY;

#ifndef A4_UI4_HEAD_API
constexpr uint8_t kCommandFlag = UiTextCommand;
struct Cursor { int x = 0, y = 0, w = 0, h = 0; bool visible = false; };
Cursor cursor(Run &h) {
    const auto &c = h.board.console_cursor_state();
    return {c.x, c.y, c.width, c.height, c.visible};
}
bool layout_on(Run &h) { return h.rt->ui()->console_layout(); }
#else
constexpr uint8_t kCommandFlag = 8;
struct Cursor { int x = 0, y = 0, w = 0, h = 0; bool visible = false; };
Cursor cursor(Run &) { return {}; }
bool layout_on(Run &) { return false; }
#endif

struct Line { std::string text; uint8_t flags; UiTextChannel channel; };
struct Geometry { int cell_w, line_h; size_t rows, cols; };
Geometry geometry(Run &h, bool context = false) {
    const auto m = tdeck::ui_text_metrics(h.rt->device_settings().ui_size);
    size_t cols = 0, rows = 0;
    tdeck::world_transcript_geometry(h.rt->device_settings().ui_size, context, cols, rows);
    return {m.cell_width, m.line_height, rows, cols};
}
std::vector<Line> lines(Run &h, const Geometry &g) {
    UiRenderedLine buf[tdeck::kAlphaTranscriptLines]{};
    const size_t k = h.rt->ui()->visible_lines(buf, g.rows, g.cols);
    std::vector<Line> out;
    for (size_t i = 0; i < k; ++i) out.push_back({buf[i].text, buf[i].flags, buf[i].channel});
    return out;
}
bool is_command(const Line &l) { return (l.flags & kCommandFlag) != 0; }
int row_y(const Geometry &g, size_t row) { return kY + int(row) * g.line_h; }

/** The bullet cell's look: only black / band blue / white; white at its top
 *  left and its tip, blue down its base, black at its top right; mirrored top
 *  to bottom. "" when it holds, else what failed. */
std::string bullet_fault(int x, int y, int w, int hgt) {
    int blue = 0, white = 0;
    for (int r = 0; r < hgt; ++r)
        for (int c = 0; c < w; ++c) {
            const uint16_t p = px(x + c, y + r);
            if (p != kBlack && p != kBand && p != kWhite) return "a foreign colour";
            if (p != px(x + c, y + hgt - 1 - r)) return "not mirrored";
            blue += p == kBand;
            white += p == kWhite;
        }
    if (px(x, y) != kWhite) return "top-left not white";
    if (px(x + w - 1, y) != kBlack) return "top-right not black";
    if (px(x + w - 1, y + hgt / 2) != kWhite) return "tip not white";
    if (px(x, y + hgt / 2) != kBand) return "base not blue";
    if (!blue || !white) return "missing a tone";
    return "";
}
bool row_has(int y, int hgt, uint16_t colour, int x0 = kX, int w = 134) {
    for (int r = 0; r < hgt; ++r)
        for (int c = 0; c < w; ++c)
            if (px(x0 + c, y + r) == colour) return true;
    return false;
}
/** The wave glyph IBM.CH 0x05 + phase, cut to w and stretched to h, at (x, y)? */
int wave_mismatch(int x, int y, int w, int hgt, int phase) {
    int bad = 0;
    for (int r = 0; r < hgt; ++r)
        for (int c = 0; c < w; ++c) {
            const int sc = w <= 8 ? c : c * 8 / w;
            bad += px(x + c, y + r) != (ibm_bit(char(0x05 + phase), r * 8 / hgt, sc) ? kWhite : kBlack);
        }
    return bad;
}
int phase_now() { return int((uint64_t(Run::now()) / 1000U / 110U) & 3U); }

/** Grass with no animated tile in its 13 x 13 surroundings (a3_04f's "nothing
 *  animated" rule), so a frame there is still unless something changes. */
bool still_spot(int &sx, int &sy) {
    const auto m = get_active_map(pack->world, MapId{LocationId(0), 0});
    if (m.error != Error::None) return false;
    for (int y = 60; y < 200; ++y)
        for (int x = 60; x < 200; ++x) {
            bool ok = m.value.tile_at(x, y) == 5;
            for (int dy = -6; dy <= 6 && ok; ++dy)
                for (int dx = -6; dx <= 6 && ok; ++dx)
                    ok = tile_animation_kind(m.value.tile_at(x + dx, y + dy)) == TileAnimationKind::Static;
            if (ok) {
                sx = x;
                sy = y;
                return true;
            }
        }
    return false;
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
void page(Run &h, bool up) {
    openu5_host_virtual_clock_us() += 100000;
    tdeck::RawInputEvent e{};
    e.kind = up ? RawInputKind::TrackballUp : RawInputKind::TrackballDown;
    e.modifiers.shift = true;
    h.raw(e);
    h.render();
}

// ---------------------------------------------------------------------------
void test_console() {
    std::printf("\nC  the console (section 8.20)\n");
    {
        Run h({{"Kojac", 'G', 120}});
        h.key(' ');
        h.run(100);
        const auto g = geometry(h);
        const auto l = lines(h, g);
        const size_t cnt = l.size();
        // The rows: ..., blank, (bullet)Pass, blank, (bullet) -- the live prompt.
        const bool shape = cnt >= 4 && l[cnt - 1].text.empty() && is_command(l[cnt - 1]) && l[cnt - 2].text.empty() &&
                           !is_command(l[cnt - 2]) && l[cnt - 3].text == "Pass" && is_command(l[cnt - 3]) &&
                           l[cnt - 4].text.empty() && !is_command(l[cnt - 4]);
        check(layout_on(h) && shape,
              "C1 the device's console: \"Pass\" on a bullet row after a blank row (getkey's LF), then a blank row "
              "and the live prompt row the next command lands on");
        const int last = row_y(g, g.rows - 1), pass = row_y(g, g.rows - 3);
        const std::string b_prompt = bullet_fault(kX, last, g.cell_w, g.line_h);
        const std::string b_pass = bullet_fault(kX, pass, g.cell_w, g.line_h);
        check(b_prompt.empty() && b_pass.empty() && !row_has(row_y(g, g.rows - 2), g.line_h, kWhite) &&
                  !row_has(row_y(g, g.rows - 4), g.line_h, kWhite),
              "C2 on the panel: the two-tone bullet (band blue, white edge) starts the Pass row and the prompt row; "
              "the rows between are blank (" + (b_prompt.empty() ? b_pass : b_prompt) + ")");
        dump("c2-console");
        bool foreign_blue = false;
        for (size_t i = 0; i < g.rows; ++i) {
            const int y = row_y(g, i);
            if (row_has(y, g.line_h, kBand, kX + g.cell_w, 134 - g.cell_w)) foreign_blue = true;
        }
        check(!foreign_blue, "C3 the blue lives in the bullet cell alone: no other cell of the console carries it");
        // Bottom anchoring: a fresh console fills upward from its last row.
        const size_t used = cnt;
        check(used < g.rows && !row_has(kY, g.line_h * int(g.rows - used), kWhite) &&
                  row_has(last, g.line_h, kWhite),
              "C4 the rows are anchored at the bottom: " + n(long(used)) + " lines on " + n(long(g.rows)) +
                  " rows, the top " + n(long(g.rows - used)) + " empty");
        // The wait cursor: after the prompt row's bullet, the wave glyph.
        const auto c = cursor(h);
        const int p0 = phase_now();
        check(c.visible && c.x == kX + g.cell_w && c.y == last && c.w == g.cell_w && c.h == g.line_h &&
                  wave_mismatch(c.x, c.y, c.w, c.h, p0) == 0,
              "C5 a command is awaited: the flame-wave cursor (IBM.CH 0x05 + phase, from the pack) sits right "
              "after the prompt row's bullet");
        // It moves: one glyph further every 110 ms, drawing that cell alone.
        bus::reset_stats();
        int phases_seen = 0, faults = 0, last_phase = p0;
        for (int t = 0; t < 4; ++t) {
            h.run(110);
            const int p = phase_now();
            phases_seen += p != last_phase;
            faults += wave_mismatch(c.x, c.y, c.w, c.h, p) != 0;
            last_phase = p;
        }
        int console_windows = 0, wide = 0;
        for (const auto &w : bus::windows())
            if (w.x0 >= kX && w.y0 >= kY) {
                ++console_windows;
                wide += w.width() > g.cell_w || w.height() > g.line_h;
            }
        check(phases_seen == 4 && faults == 0 && console_windows >= 4 && wide == 0,
              "C6 the wave animates (4 phase changes in 440 ms, each the right glyph) and an idle frame draws "
              "the cursor's one cell, no row (" + n(console_windows) + " console windows, " + n(wide) + " wider)");
        // Shift+Up pages back: the newest row is off screen, so is the cursor.
        for (int i = 0; i < 12; ++i) {
            h.key(' ');
            h.run(100);
        }
        page(h, true);
        const bool hidden = !cursor(h).visible;
        page(h, false);
        h.run(20);
        check(hidden && cursor(h).visible, "C7 a scrolled-back view shows no cursor; paging back down brings it back");
    }
    {
        // A still frame: nothing animated in view, nothing changed -- the
        // runtime's idle path alone moves the wave (Board::animate_console_cursor).
        Run h({{"Kojac", 'G', 120}});
        int sx = -1, sy = -1;
        DebugTeleportRequest r{};
        r.kind = DebugDestinationKind::Britannia;
        const bool found = still_spot(sx, sy);
        r.x = sx;
        r.y = sy;
        const bool moved = found && apply_debug_teleport(h.rt->command_context_for_test(), r).status ==
                                        DebugTeleportStatus::Applied;
        h.render(true);
        h.key(' ');
        h.run(500);
        const auto c = cursor(h);
        bus::reset_stats();
        int faults = 0, changes = 0, last = phase_now();
        for (int t = 0; t < 4; ++t) {
            h.run(110);
            const int p = phase_now();
            changes += p != last;
            faults += wave_mismatch(c.x, c.y, c.w, c.h, p) != 0;
            last = p;
        }
        int windows = 0, other = 0;
        for (const auto &w : bus::windows()) {
            ++windows;
            other += w.x0 != c.x || w.y0 != c.y || w.width() != c.w || w.height() != c.h;
        }
        check(moved && c.visible && changes == 4 && faults == 0 && windows >= 4 && other == 0,
              "C6b on a still screen (grass, nothing animated within 6 cells) the wave still moves: the idle "
              "frames draw the cursor's cell and nothing else (" + n(windows) + " windows, " + n(other) + " elsewhere)");
    }
    {
        // getdir: the cursor waits after the echo, on its own row; no prompt row.
        Run h({{"Kojac", 'G', 120}});
        h.key('l');
        h.run(50);
        const auto g = geometry(h);
        const auto l = lines(h, g);
        const auto c = cursor(h);
        const bool echo_last = !l.empty() && l.back().text == "Look-" && is_command(l.back());
        check(echo_last && c.visible && c.x == kX + 6 * g.cell_w && c.w == g.cell_w &&
                  bullet_fault(kX, c.y, g.cell_w, g.line_h).empty() && wave_mismatch(c.x, c.y, c.w, c.h, phase_now()) == 0,
              "C8 getdir 0x35EC: \"Look-\" is the last row and the cursor blinks right after it (no prompt row below)");
        dump("c8-getdir");
        h.up();
        h.run(50);
        const auto l2 = lines(h, g);
        bool look_north = false;
        for (const auto &x : l2) look_north = look_north || (x.text == "Look-North" && is_command(x));
        const auto c2 = cursor(h);
        check(look_north && c2.visible && c2.x == kX + g.cell_w,
              "C8b the direction completes the same bullet row (\"Look-North\") and the prompt row returns");
    }
    {
        // An answer echoed on its own row has no bullet and no blank above it.
        Run h({{"Kojac", 'G', 120}});
        h.key('t');
        h.up();
        h.run(50);
        const auto g = geometry(h);
        const auto l = lines(h, g);
        bool shape = false;
        for (size_t i = 0; i + 1 < l.size(); ++i)
            if (l[i].text == "Talk-" && is_command(l[i]))
                shape = !l[i + 1].text.empty() && !is_command(l[i + 1]) && l[i + 1].text != "Talk-";
        check(shape, "C9 Talk's direction word (an answer, D-75's open row) follows \"Talk-\" with no bullet and "
                     "no blank row");
    }
    {
        // A busy hold (Mix's 10-tick wait, CMDS 0x1b88): no prompt row, no cursor.
        Run h({{"Kojac", 'G', 120}}, true);
        auto &g = h.rt->game();
        const auto *def = spell_definition(SpellId(0));
        for (auto &r : g.reagent_quantities) r = 0;
        for (int i = 0; i < 8; ++i)
            if (def && (def->reagents & (1 << i))) g.reagent_quantities[i] = 5;
        h.key('m');
        h.key('\r');
        for (int i = 0; i < 8; ++i)
            if (def && (def->reagents & (1 << i))) {
                h.key('\r');
                h.down();
            }
        h.key('m');
        h.key('1');
        h.key('\r', false, false, 1000);
        h.run(100);
        const auto geo = geometry(h);
        auto l = lines(h, geo);
        const bool held = !l.empty() && l.back().text == "Mixing..." && !cursor(h).visible;
        h.run(800);
        l = lines(h, geo);
        const bool back = l.size() >= 2 && l.back().text.empty() && is_command(l.back()) && cursor(h).visible;
        check(held && back, "C10 while Mix holds its 10 ticks the console shows no prompt row and no cursor; after "
                            "\"Done!\" both return");
    }
    for (int size : {0, 2}) {
        Run h({{"Kojac", 'G', 120}});
        set_text_size(h, size);
        h.key(' ');
        h.run(100);
        const auto g = geometry(h);
        const int last = row_y(g, g.rows - 1);
        const auto c = cursor(h);
        const std::string fault = bullet_fault(kX, last, g.cell_w, g.line_h);
        check(h.rt->device_settings().ui_size == size && fault.empty() && c.visible && c.x == kX + g.cell_w &&
                  c.y == last && c.w == g.cell_w && c.h == g.line_h && wave_mismatch(c.x, c.y, c.w, c.h, phase_now()) == 0,
              std::string(size ? "C11b Large" : "C11 Small") + " text: the bullet fills its " + n(g.cell_w) + "x" +
                  n(g.line_h) + " cell and the wave cursor follows it on the last row (" + fault + ")");
        dump(size ? "c11-large" : "c11-small");
        set_text_size(h, 1);
    }
#ifndef A4_UI4_HEAD_API
    {
        // The layout is the device's: a plain session wraps as it always did;
        // with it on, a command row is one column short (the bullet's cell).
        UiTextBlock a[16]{}, b[16]{};
        UiSession plain({a, 16}, {}, {22, 19, 63}), console({b, 16}, {}, {22, 19, 63});
        console.set_console_layout(true);
        for (auto *s : {&plain, &console}) {
            s->append(UiTextChannel::CommandEcho, "abc defgh ijklm nopqrs", UiTextCommand);
            s->append(UiTextChannel::CommandEcho, "ABCDEFGHIJKLMNOPQRSTUV", UiTextCommand);
            s->append(UiTextChannel::CommandEcho, "North");
        }
        UiRenderedLine pl[19]{}, cl[19]{};
        const size_t pn = plain.visible_lines(pl, 19, 22), cn = console.visible_lines(cl, 19, 22);
        const bool plain_same = pn == 3 && std::string(pl[0].text) == "abc defgh ijklm nopqrs" &&
                                std::string(pl[1].text) == "ABCDEFGHIJKLMNOPQRSTUV" && std::string(pl[2].text) == "North" &&
                                !(pl[0].flags & UiTextCommand) && !(pl[1].flags & UiTextCommand);
        const char *want[] = {"", "abc defgh ijklm", "nopqrs", "", "ABCDEFGHIJKLMNOPQRSTU", "V", "North"};
        const bool flags[] = {false, true, false, false, true, false, false};
        bool console_ok = cn == 7;
        for (size_t i = 0; console_ok && i < 7; ++i)
            console_ok = std::string(cl[i].text) == want[i] && ((cl[i].flags & UiTextCommand) != 0) == flags[i];
        console.set_console_ready(true);
        const size_t ready = console.visible_lines(cl, 19, 22);
        check(plain_same && console_ok && ready == 9 && console.console_cursor() == UiConsoleCursor::NewCommand,
              "C12 the layout is the device's alone: a plain UiSession's lines are unchanged; with it, a command "
              "wraps one column short behind a blank row, an answer gets neither, and \"ready\" adds the prompt rows");
    }
#else
    check(false, "C12 the console layout (new API: UiSession::set_console_layout)");
#endif
    {
        // The arena's cast: the key prints nothing; combat_cast()'s "Cast...\n"
        // (combat.cpp, the combat parity pin) is its one echo, once the spell is
        // chosen. Two would read as two commands on the bulleted console.
        UiTextBlock k[16]{};
        UiSession arena({k, 16}, {}, {22, 19, 63});
        GameEvent start{};
        start.kind = GameEventKind::CombatStarted;
        arena.consume(start);
        const size_t before = arena.transcript_size();
        UiAction c{};
        c.kind = UiActionKind::Character;
        c.character = u'c';
        const bool handled = arena.handle_input(c);
        check(arena.base_mode() == UiMode::Combat && handled && arena.transcript_size() == before,
              "C13 the arena's (C)ast key echoes nothing itself: combat_cast's own \"Cast...\" is printed once");
    }
}

// ---------------------------------------------------------------------------
// H: the console's height. The hardware photo (section 8.22): twelve rows at
// the bottom of a 19-row black area -- UiSession's page_rows (12, the
// device's constructor config) capped visible_lines() below the Board's
// geometry, so the top seven rows were never used and Shift+Up paged by 19
// while 12 were shown.
struct HeightCase { int size; const char *name; size_t rows, bar_rows; int bottom; };
constexpr HeightCase kHeights[] = {
    {0, "Small", 21, 18, 88 + 21 * 7},  // 5x7 cells: 235, glass at 240
    {1, "Medium", 19, 15, 88 + 19 * 8}, // 6x8: 240
    {2, "Large", 15, 12, 88 + 15 * 10}, // 8x10: 238
};
/** Every console row of the panel inked exactly where its line has content. */
size_t row_faults(const Geometry &g, const std::vector<Line> &l) {
    size_t bad = 0;
    for (size_t i = 0; i < g.rows; ++i) {
        const bool want = i < l.size() && (!l[i].text.empty() || is_command(l[i]));
        bad += row_has(row_y(g, i), g.line_h, kWhite) != want;
    }
    return bad;
}
/** Console row windows of the last draws: x 184..318 (x 319 is the edge's own
 *  fill), starting at or below the rule; how many start above y 88 or run past
 *  `bottom`. */
int stray_windows(int bottom, int &seen) {
    int stray = 0;
    seen = 0;
    for (const auto &w : bus::windows()) {
        if (w.x0 < kX || w.x0 >= 319 || w.y1 < kY - 1 || w.y0 >= bottom) continue;
        ++seen;
        const bool out = w.y0 < kY || w.y1 >= bottom;
        if (out) std::printf("      stray window x %d..%d y %d..%d\n", w.x0, w.x1, w.y0, w.y1);
        stray += out;
    }
    return stray;
}
/** The System Menu's round trip: closing it repaints the whole gameplay
 *  screen (every console row included), the windows of which stay logged. */
void full_repaint(Run &h) {
    h.key('m', true);
    h.run(20);
    bus::reset_stats();
    h.mic();
}

void test_console_height() {
    std::printf("\nH  the console's height (section 8.22)\n");
    for (const auto &c : kHeights) {
        Run h({{"Kojac", 'G', 120}});
        if (c.size != 1) set_text_size(h, c.size);
        for (int i = 0; i < 26; ++i) { // 52 rows of history: two pages at any size
            h.key(' ');
            h.run(20);
        }
        h.run(100);
        const auto g = geometry(h);
        full_repaint(h);
        const auto l = lines(h, g);
        int seen = 0;
        const int stray = stray_windows(240, seen);
        const int last = row_y(g, g.rows - 1);
        const auto cur = cursor(h);
        const std::string tag = std::string(c.name) + " ";
        check(h.rt->device_settings().ui_size == c.size && g.rows == c.rows && l.size() == c.rows &&
                  row_faults(g, l) == 0,
              "H1 " + tag + "a full console uses all " + n(long(c.rows)) + " rows from y 88 (" + n(long(l.size())) +
                  " lines drawn, " + n(long(row_faults(g, l))) + " rows inked wrongly)");
        check(row_y(g, g.rows) == c.bottom && c.bottom <= 240 && seen > 0 && stray == 0,
              "H2 " + tag + "its rows run from y 88 to y " + n(c.bottom) + " (glass 240): no console window above "
              "the status box's rule or past the glass (" + n(seen) + " windows, " + n(stray) + " stray)");
        check(l.back().text.empty() && is_command(l.back()) && bullet_fault(kX, last, g.cell_w, g.line_h).empty() &&
                  cur.visible && cur.x == kX + g.cell_w && cur.y == last && cur.y + cur.h <= 240 &&
                  wave_mismatch(cur.x, cur.y, cur.w, cur.h, phase_now()) == 0,
              "H3 " + tag + "bottom-anchored: the live prompt row is the last row (y " + n(last) + "), its bullet "
              "and the wave cursor fit inside it");
        dump(c.size == 0 ? "h-small" : c.size == 1 ? "h-medium" : "h-large");
        page(h, true);
        const auto back = lines(h, g);
        const bool scrolled = h.rt->ui()->scroll_offset_lines() == c.rows && back.size() == c.rows &&
                              !cursor(h).visible && row_faults(g, back) == 0;
        page(h, false);
        h.run(20);
        const auto again = cursor(h);
        check(scrolled && h.rt->ui()->scroll_offset_lines() == 0 && again.visible && again.y == last,
              "H4 " + tag + "Shift+Up pages back by exactly the " + n(long(c.rows)) + " rows drawn (no line skipped), "
              "full, no cursor; Shift+Down returns the cursor to the last row");
        // A context bar (getdir's Look-): the rows stop above it.
        h.key('l');
        h.run(50);
        const auto gb = geometry(h, true);
        full_repaint(h);
        const auto lb = lines(h, gb);
        const auto cb = cursor(h);
        const int stray_bar = stray_windows(tdeck::kContextBarTop, seen);
        check(gb.rows == c.bar_rows && lb.size() == c.bar_rows && row_faults(gb, lb) == 0 && stray_bar == 0 &&
                  row_y(gb, gb.rows) <= tdeck::kContextBarTop && lb.back().text == "Look-" && cb.visible &&
                  cb.y == row_y(gb, gb.rows - 1) && cb.y + cb.h <= tdeck::kContextBarTop,
              "H5 " + tag + "with the context bar up (Look-) the console uses " + n(long(c.bar_rows)) + " rows, ending "
              "at y " + n(row_y(gb, gb.rows)) + " above the bar (y " + n(tdeck::kContextBarTop) + "), the cursor after "
              "\"Look-\" on its last row");
        dump(c.size == 0 ? "h-small-bar" : c.size == 1 ? "h-medium-bar" : "h-large-bar");
        h.up();
        h.run(50);
    }
}

// ---------------------------------------------------------------------------
void test_lists() {
    std::printf("\nP  the lists in reverse video (section 8.20)\n");
    {
        Run h({{"Kojac", 'G', 120}, {"Iolo", 'G', 110}, {"Shamino", 'G', 100}});
        h.key('r');
        h.run(50);
        auto bar = [](int row) { return px(184, 44 + row * 14) == kWhite && px(317, 44 + row * 14 + 13) == kWhite; };
        auto clear = [](int row) {
            const int y = 44 + row * 14;
            for (int c = 184; c <= 317; ++c)
                for (int r : {0, 1, 9, 10, 11, 12, 13})
                    if (px(c, y + r) != kBlack) return false;
            return true;
        };
        // The context bar's key legend stays green (its own design); the list is not.
        const bool ink0 = row_has(46, 7, kBlack, 190, 120), green = row_has(44, 112, kGreen, 182, 137);
        check(h.rt->ui()->mode() == UiMode::PartySelection && bar(0) && ink0 && !bar(1) && clear(1) && !green,
              "P1 the party picker selects in reverse video: row 0 is a white 14 px bar with black ink, the others "
              "white on black, no green anywhere in the panel");
        dump("p1-picker");
        h.down();
        h.run(50);
        check(bar(1) && !bar(0) && clear(0) && clear(2),
              "P2 moving down moves the bar; the row it left is black on its whole pitch (nothing stale)");
        h.mic();
        h.run(50);
    }
    {
        Run h({{"Kojac", 'G', 120}});
        tdeck::DeviceShopView shop{};
        shop.active = true;
        std::snprintf(shop.title, sizeof(shop.title), "Arms");
        std::snprintf(shop.keeper, sizeof(shop.keeper), "Keeper");
        std::snprintf(shop.phase, sizeof(shop.phase), "Buy");
        shop.gold = 400;
        shop.row_count = shop.total = 3;
        const char *names[] = {"Dagger", "Sling", "Club"};
        for (size_t i = 0; i < 3; ++i) {
            std::snprintf(shop.rows[i].name, sizeof(shop.rows[i].name), "%s", names[i]);
            shop.rows[i].price = int32_t(10 + i);
            shop.rows[i].quantity = 1;
        }
        shop.selected_row = 1;
        std::vector<uint16_t> view(size_t(openu5::kViewportPixels) * openu5::kViewportPixels, 0);
        openu5::TurnState turn{};
        openu5::HudWorldState hud{};
        const esp_err_t e = h.board.show_alpha(view.data(), *h.rt->ui(), h.rt->game(), turn, hud, pack->runes_font,
                                               nullptr, nullptr, false, nullptr, false, 1, &shop);
        auto bar = [](int row) { return px(184, 37 + row * 14) == kWhite && px(317, 37 + row * 14 + 13) == kWhite; };
        check(e == ESP_OK && bar(1) && !bar(0) && !bar(2) && row_has(39 + 14, 7, kBlack, 190, 120) &&
                  !row_has(37, 84, kGreen, 182, 137),
              "P3 a shop's offers select in reverse video too: the chosen offer is the bar, no green '>'");
        dump("p3-shop");
    }
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: a4_ui4_console_runtime <pack> <openu5-audio.bin> [--dump <dir>]\n");
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
    test_console();
    test_console_height();
    test_lists();
    std::printf("A4-UI4 console runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
