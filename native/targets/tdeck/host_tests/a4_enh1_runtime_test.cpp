// Alpha 4 A4-ENH1 (ALPHA4_UI.md section 10) -- the enhancement pass on the
// REAL AlphaRuntime (A3-HF4's source set: the capture Board, the memory card,
// the virtual clock).
//
//   a4_enh1_runtime <openu5-alpha1-resources.bin>
//
//   W  the trackball click: an immediate one-press WASD toggle on every
//      screen, with its transcript line, and never a key for what is open
//   T  Developer > Diagnostics > "Trackball stats (live)"
//   S  the trackball speed levels through the device's own Settings row
//   D  the Developer menu: no ordinary menu row, Alt+D everywhere
//   C  the Cheats page: each cheat through the device's menu, God Mode in
//      play, the save's sidecar, Alt+L, and the refusal in combat
//   R  the Difficulty page: the presets, Easy's poison cadence in play, and
//      the preset kept by a save and Alt+L
#include "../main/alpha_runtime.h"
#include "../main/tdeck_board.h"
#include "esp_timer.h"
#include "openu5/frontend_settings.h"
#include "openu5/outdoor.h"
#include "openu5/save_json.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

using namespace openu5;
using tdeck::RawInputKind;

namespace tdeck {
void host_memory_save_forget_for_test();
bool host_memory_save_edit_for_test(bool newest, void (*edit)(openu5::save::Json &game_state));
std::string &a3_host_settings_text();
bool &a3_host_settings_enabled();
} // namespace tdeck
void batch37_reset_screen();

namespace {
int checks = 0, failures = 0;
bool check(bool good, const char *id, const std::string &label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s %s\n", good ? "GREEN" : "RED", id, label.c_str());
    return good;
}
const tdeck::AlphaResourceOwners *pack = nullptr;
constexpr uint8_t kCastle = 17;

struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    size_t mark = 0;
    Run() {
        openu5_host_virtual_clock_us() = 5'000'000;
        batch37_reset_screen();
        tdeck::host_memory_save_forget_for_test();
        tdeck::AlphaRuntime::HostTestFixture f{};
        f.world = pack->world;
        f.npc_locations = pack->npc_locations;
        f.pack = pack;
        f.location_x = pack->location_x;
        f.location_y = pack->location_y;
        f.location_count = pack->location_count;
        f.render_pixels = true;
        f.indexed_test_tiles = true;
        rt->attach_host_test_fixture(f);
        auto &g = rt->game();
        g.position.map = {0, 0};
        g.position.xy = {pack->location_x[kCastle - 1], pack->location_y[kCastle - 1]};
        g.position.xy.y = uint8_t(g.position.xy.y + 2);
        g.time.hour = 12;
        g.food = 80;
        g.gold = 100;
        g.party.character_count = g.party.party_size = 1;
        g.party.active_character = 255;
        auto &m = g.party.characters[0];
        std::snprintf(m.name, sizeof(m.name), "Avatar");
        m.status = 'G';
        m.party_status = 0;
        m.character_class = 'A';
        m.level = 1;
        m.current_hp = m.max_hp = 300;
        m.strength = m.dexterity = m.intelligence = 30;
        frames(3);
    }
    GameState &g() { return rt->game(); }
    const UiSession &ui() { return *rt->ui(); }
    UiMode mode() { return ui().mode(); }
    void frames(int n) {
        for (int i = 0; i < n; ++i) {
            openu5_host_virtual_clock_us() += 5000;
            rt->render(board);
        }
    }
    void raw(tdeck::RawInputEvent e) {
        e.timestamp_us = openu5_host_virtual_clock_us();
        rt->handle(e);
        frames(1);
    }
    void key(uint8_t code, bool alt = false) {
        openu5_host_virtual_clock_us() += 100000;
        tdeck::RawInputEvent e{};
        e.kind = RawInputKind::Keyboard;
        e.transition = tdeck::KeyTransition::Pressed;
        e.code = e.base_code = code;
        e.modifiers.alt = alt;
        raw(e);
    }
    void mic_hold() {
        openu5_host_virtual_clock_us() += 100000;
        tdeck::RawInputEvent e{};
        e.kind = RawInputKind::Keyboard;
        e.column = tdeck::kMicrophoneKeyColumn;
        e.row = tdeck::kMicrophoneKeyRow;
        e.transition = tdeck::KeyTransition::Pressed;
        raw(e);
        for (int i = 0; i < 240; ++i) frames(1); // 1.2 s: the hold fires in render()'s pump
        e.transition = tdeck::KeyTransition::Released;
        raw(e);
    }
    void ball(RawInputKind k, int64_t advance_us = 100000) {
        openu5_host_virtual_clock_us() += advance_us;
        tdeck::RawInputEvent e{};
        e.kind = k;
        raw(e);
    }
    /** One physical click: press, 1 ms bounce (release + press), release 120 ms later. */
    void click(bool bounce = true) {
        openu5_host_virtual_clock_us() += 200000;
        tdeck::RawInputEvent e{};
        e.kind = RawInputKind::TrackballClick;
        e.transition = tdeck::KeyTransition::Pressed;
        raw(e);
        if (bounce) {
            openu5_host_virtual_clock_us() += 1000;
            e.transition = tdeck::KeyTransition::Released;
            raw(e);
            openu5_host_virtual_clock_us() += 1000;
            e.transition = tdeck::KeyTransition::Pressed;
            raw(e);
        }
        openu5_host_virtual_clock_us() += 120000;
        e.transition = tdeck::KeyTransition::Released;
        raw(e);
    }
    void set_mark() { mark = ui().transcript_size(); }
    int count_since(const char *needle) {
        int n = 0;
        for (size_t i = mark; i < ui().transcript_size(); ++i)
            if (const auto *b = ui().transcript_at(i); b && std::strstr(b->text, needle)) ++n;
        return n;
    }
    std::string last_line() {
        const size_t n = ui().transcript_size();
        const auto *b = n ? ui().transcript_at(n - 1) : nullptr;
        return b ? b->text : "";
    }
};

std::string report(tdeck::AlphaRuntime &rt) {
    std::string out;
    for (size_t i = 0; i < rt.perf_report_line_count(); ++i) out += std::string(rt.perf_report_line(i)) + "\n";
    return out;
}

void test_click() {
    Run h;
    tdeck::a3_host_settings_enabled() = true;
    tdeck::a3_host_settings_text() = "untouched";
    const auto pos = h.g().position;
    const uint32_t routed = h.rt->routed_command_count();
    h.set_mark();
    h.click();
    check(h.rt->movement_mode() && h.count_since("WASD Mode: ON") == 1 && h.count_since("WASD Mode: OFF") == 0,
          "W1", "one click (with a 1 ms contact bounce) turns WASD Mode ON once, with one transcript line");
    check(h.rt->routed_command_count() == routed && h.g().position.xy.x == pos.xy.x &&
              h.g().position.xy.y == pos.xy.y && h.mode() == UiMode::Exploration,
          "W2", "the click routed no command and moved nothing; the UI mode is unchanged");
    check(tdeck::a3_host_settings_text() == "untouched", "W3",
          "the live toggle writes no settings.json (\"Movement default\" stays the Settings row's)");
    h.key('w');
    check(h.g().position.xy.y == uint8_t(pos.xy.y - 1), "W4", "with WASD on, W walks north");
    h.set_mark();
    h.click();
    check(!h.rt->movement_mode() && h.count_since("WASD Mode: OFF") == 1 && h.count_since("WASD Mode: ON") == 0,
          "W5", "the next click turns it OFF, once");
    // A press held for 2 s: one toggle at the press edge, nothing on the release.
    h.set_mark();
    openu5_host_virtual_clock_us() += 300000;
    tdeck::RawInputEvent e{};
    e.kind = RawInputKind::TrackballClick;
    e.transition = tdeck::KeyTransition::Pressed;
    h.raw(e);
    const bool on_at_press = h.rt->movement_mode();
    h.frames(400);
    e.transition = tdeck::KeyTransition::Released;
    h.raw(e);
    check(on_at_press && h.rt->movement_mode() && h.count_since("WASD Mode") == 1, "W6",
          "a 2 s hold toggles at the press edge, before the release, and only once");
    h.click();
    // The System Menu: the click toggles, the menu keeps its page and row.
    h.key('m', true);
    const auto before = h.rt->system_menu_view();
    h.click();
    const auto after = h.rt->system_menu_view();
    check(h.rt->system_menu_open() && after.selected_line == before.selected_line &&
              std::string(after.title) == before.title && h.rt->movement_mode(),
          "W7", "in the System Menu the click toggles WASD and selects nothing");
    h.key('m', true);
    h.click();
    // A prompt waiting for a direction: the click neither answers nor cancels it.
    h.key('l');
    const UiMode prompt = h.mode();
    const uint32_t routed_look = h.rt->routed_command_count();
    h.click();
    check(prompt == UiMode::TargetSelection && h.mode() == UiMode::TargetSelection &&
              h.rt->routed_command_count() == routed_look,
          "W8", "Look's Direction? stays open across a click (no direction, no Enter, no Cancel)");
    h.ball(RawInputKind::TrackballDown);
    check(h.mode() == UiMode::Exploration, "W9", "and the roll after it still answers the prompt");
    // The Mic hold toggles the same mode and says so the same way.
    h.set_mark();
    const bool was = h.rt->movement_mode();
    h.mic_hold();
    check(h.rt->movement_mode() != was && h.count_since(was ? "WASD Mode: OFF" : "WASD Mode: ON") == 1, "W10",
          "the 1.1 s Mic hold toggles the same mode, with the same line");
    // The title: the click activates no menu item.
    h.key('m', true);
    for (int i = 0; i < 12 && h.rt->system_menu_view().selected_line + 1 != int(h.rt->system_menu_view().line_count); ++i)
        h.ball(RawInputKind::TrackballDown);
    h.key('\r');
    h.key(' ');
    const auto state = h.rt->frontend_state();
    const auto menu = h.rt->frontend_view();
    h.click();
    check(h.rt->frontend_open() && h.rt->frontend_state() == state &&
              h.rt->frontend_view().selected_line == menu.selected_line,
          "W11", "on the title the click toggles WASD and activates nothing");
    tdeck::a3_host_settings_enabled() = false;
}

void test_report() {
    Run h;
    // Rolls: four pulses east 40 ms apart, a 2 ms bounce; then a click.
    for (int i = 0; i < 4; ++i) h.ball(RawInputKind::TrackballRight, 40000);
    h.ball(RawInputKind::TrackballRight, 2000);
    h.frames(100); // > 400 ms quiet: the roll closes
    h.click();
    h.key('d', true);
    for (int i = 0; i < 2; ++i) h.ball(RawInputKind::TrackballUp); // root: Diagnostics is two above row 0
    h.key('\r');
    for (int i = 0; i < 6; ++i) h.ball(RawInputKind::TrackballUp); // six above the first
    h.key('\r');
    const std::string text = report(*h.rt);
    check(h.rt->perf_report_open() && text.find("TRACKBALL STATS") == 0 &&
              text.find("Pulses  U ") != std::string::npos && text.find("R 5") != std::string::npos &&
              text.find("Clicks  pressed 2  toggled 1  bounce 1") != std::string::npos &&
              text.find(" R5>4 ") != std::string::npos,
          "T1", "Developer > Diagnostics, six rows up: the trackball report (pulses, steps, clicks, last rolls):\n" + text);
    h.key('\r');
    h.key('\r');
    const std::string again = report(*h.rt);
    check(again.find("Rolls   0") != std::string::npos, "T2",
          "reading it started a new window (the next read counts only what came after):\n" + again);
}
void test_speed() {
    tdeck::a3_host_settings_enabled() = true;
    tdeck::a3_host_settings_text().clear();
    {
        Run h; // the fixture runs speed 10, one step per pulse (the older tests' contract)
        // S1. System Menu > Settings > the trackball row, five to the left: 5.
        h.key('m', true);
        for (int i = 0; i < 3; ++i) h.ball(RawInputKind::TrackballDown);
        h.key('\r');
        for (int i = 0; i < 2; ++i) h.ball(RawInputKind::TrackballDown);
        const std::string row10 = h.rt->system_menu_view().lines[2];
        // Each edit applies at once, so the trackball changing its own speed
        // feels it: 10 -> 6 one pulse a step, then 6 -> 5 takes two.
        for (int i = 0; i < 4; ++i) h.ball(RawInputKind::TrackballLeft);
        const std::string row6 = h.rt->system_menu_view().lines[2];
        h.ball(RawInputKind::TrackballLeft); // 100 ms: past speed 6's 80 ms step gap
        const std::string row6_still = h.rt->system_menu_view().lines[2];
        h.ball(RawInputKind::TrackballLeft);
        const std::string row5 = h.rt->system_menu_view().lines[2];
        const std::string footer = h.rt->system_menu_view().footer;
        // Mic leaves Settings and saves them (System Menu convention).
        tdeck::RawInputEvent mic{};
        mic.kind = RawInputKind::Keyboard;
        mic.column = tdeck::kMicrophoneKeyColumn;
        mic.row = tdeck::kMicrophoneKeyRow;
        mic.transition = tdeck::KeyTransition::Pressed;
        openu5_host_virtual_clock_us() += 100000;
        h.raw(mic);
        mic.transition = tdeck::KeyTransition::Released;
        openu5_host_virtual_clock_us() += 100000;
        h.raw(mic);
        const std::string saved = tdeck::a3_host_settings_text();
        check(row10 == "Trackball speed: 10/10" && row6 == "Trackball speed: 6/10" && row6_still == row6 &&
                  row5 == "Trackball speed: 5/10" &&
                  footer.find("1 slow - 10 fast") != std::string::npos && h.rt->device_settings().trackball_speed == 5 &&
                  saved.find("\"trackballSpeed\":5") != std::string::npos &&
                  saved.find("\"trackballResponsiveness\":100") != std::string::npos,
              "S1", "System Menu > Settings > \"" + row10 + "\" -> \"" + row5 + "\", Mic saves: " + saved);
        // S7. In the menu at speed 5 one pulse does not move the cursor; three do.
        h.ball(RawInputKind::TrackballDown, 40000);
        const int one = h.rt->system_menu_view().selected_line;
        h.ball(RawInputKind::TrackballDown, 40000);
        h.ball(RawInputKind::TrackballDown, 40000);
        const int three = h.rt->system_menu_view().selected_line;
        check(one == 0 && three == 1, "S7", "back on the root at speed 5: one pulse leaves the cursor on Resume, "
                                            "three move it one row (" + std::to_string(one) + ", " + std::to_string(three) + ")");
        h.key('m', true);
        // S2. In the world: a tiny roll (one or two pulses) moves nothing; the
        // third pulse of a normal roll is one step.
        h.frames(100); // > 400 ms: any partial step from the menu is gone
        const uint32_t routed = h.rt->routed_command_count();
        h.ball(RawInputKind::TrackballRight, 40000);
        h.ball(RawInputKind::TrackballRight, 40000);
        const bool tiny_still = h.rt->routed_command_count() == routed;
        h.ball(RawInputKind::TrackballRight, 40000);
        check(tiny_still && h.rt->routed_command_count() == routed + 1, "S2",
              "speed 5: two pulses route nothing, the third routes one move");
        // S3. Tiny, pause, tiny: no step (the partial step is forgotten).
        h.frames(100);
        const uint32_t mid = h.rt->routed_command_count();
        h.ball(RawInputKind::TrackballLeft, 40000);
        h.ball(RawInputKind::TrackballLeft, 40000);
        h.frames(100);
        h.ball(RawInputKind::TrackballLeft, 40000);
        check(h.rt->routed_command_count() == mid, "S3", "two pulses, a 0.5 s pause, one pulse: still no move");
        // S4. A fast flick: 30 pulses 4 ms apart is a short burst of steps, and
        // nothing more once the ball stops (no queued moves play out).
        h.frames(100);
        const uint32_t before_flick = h.rt->routed_command_count();
        for (int i = 0; i < 30; ++i) {
            openu5_host_virtual_clock_us() += 4000;
            tdeck::RawInputEvent e{};
            e.kind = RawInputKind::TrackballDown;
            e.timestamp_us = openu5_host_virtual_clock_us();
            h.rt->handle(e);
        }
        h.frames(1);
        const uint32_t after_flick = h.rt->routed_command_count();
        h.frames(300);
        check(after_flick - before_flick >= 1 && after_flick - before_flick <= 3 &&
                  h.rt->routed_command_count() == after_flick,
              "S4", "a 30-pulse flick routes " + std::to_string(after_flick - before_flick) +
                        " moves (1..3), and none after the ball stops");
    }
    {
        // S5. A second boot reads the saved speed.
        Run again;
        check(again.rt->device_settings().trackball_speed == 5, "S5", "after a reboot settings.json gives speed 5");
    }
    {
        // S6. A settings.json from before A4-ENH1 (old 25 %, no speed key):
        // the default speed, whatever the old percentage said.
        tdeck::a3_host_settings_text() =
            "{\"version\":1,\"brightness\":70,\"movementMode\":false,\"trackballResponsiveness\":25,\"uiSize\":1,"
            "\"developerToolsVisible\":false,\"soundVolume\":80,\"musicVolume\":80,\"touchControls\":false}";
        Run old;
        check(old.rt->device_settings().trackball_speed == openu5::kTrackballSpeedDefault &&
                  old.rt->device_settings().trackball_responsiveness == 25 && old.rt->device_settings().brightness == 70,
              "S6", "an Alpha 2-4 settings.json loads whole; the trackball takes the default speed (5)");
    }
    tdeck::a3_host_settings_enabled() = false;
    tdeck::a3_host_settings_text().clear();
}
// The newest save's sidecar gameState, as written to the (memory) card.
openu5::save::Json g_side;
bool newest_sidecar() {
    g_side = openu5::save::Json{};
    return tdeck::host_memory_save_edit_for_test(true, [](openu5::save::Json &gs) { g_side = gs; });
}

/** System Menu > Cheats (row 5, after Difficulty) > a group (A4-ENH2: Party
 *  0, Inventory 1, ...), then `row` Downs. */
void open_cheats(Run &h, int row = 0, int group = 0) {
    h.key('m', true);
    for (int i = 0; i < 5; ++i) h.ball(RawInputKind::TrackballDown);
    h.key('\r');
    for (int i = 0; i < group; ++i) h.ball(RawInputKind::TrackballDown);
    h.key('\r');
    for (int i = 0; i < row; ++i) h.ball(RawInputKind::TrackballDown);
}

void test_cheats() {
    Run h;
    auto &m = h.g().party.characters[0];
    h.key('m', true);
    const auto root = h.rt->system_menu_view();
    check(root.line_count == 7 && std::string(root.lines[4]) == "Difficulty" && std::string(root.lines[5]) == "Cheats" &&
              std::string(root.lines[root.line_count - 1]) == "Return to Title",
          "C1", "the System Menu root: Difficulty and Cheats after Settings, Return to Title still last");
    h.key('m', true);
    open_cheats(h);
    auto v = h.rt->system_menu_view();
    // A4-ENH2 (changed on purpose): the Cheats page lists groups; God Mode,
    // Heal and Cure are on the Party page, Add Gold and Max Gold on Inventory.
    check(std::string(v.title) == "Party" && v.line_count == 5 && std::string(v.lines[0]) == "God Mode: Off" &&
              std::string(v.lines[1]) == "Heal Party" && std::string(v.lines[2]) == "Cure Party" &&
              std::string(v.lines[3]) == "Restore MP" && std::string(v.lines[4]) == "Revive Party" &&
              std::string(v.subtitle) == "Using one marks this journey's save" &&
              std::string(v.footer) == "Party members take no damage",
          "C2", "Cheats > Party: its five rows, the help line, and a fresh journey not yet marked");
    h.set_mark();
    h.key('\r');
    v = h.rt->system_menu_view();
    check(h.g().enhanced.god_mode && std::string(v.lines[0]) == "God Mode: On" && std::string(v.footer) == "God Mode: ON" &&
              std::string(v.subtitle) == "This journey has used cheats" && h.count_since("God Mode: ON") == 1 &&
              h.rt->system_menu_open(),
          "C3", "Enter: God Mode on, said in the footer and the transcript; the page stays open and is now marked");
    // Add Gold (Cheats > Inventory, its first row): right picks +1000, Enter adds it; Max Gold.
    h.key('\b');
    h.ball(RawInputKind::TrackballDown);
    h.key('\r');
    h.ball(RawInputKind::TrackballRight);
    const std::string amount = h.rt->system_menu_view().lines[0];
    h.key('\r');
    const int after_add = h.g().gold;
    h.ball(RawInputKind::TrackballDown);
    h.key('\r');
    check(amount == "Add Gold: +1000" && after_add == 1100 && h.g().gold == 9999, "C4",
          "Add Gold +1000 from 100 = 1100; Max Gold = 9999");
    // Heal and Cure through the menu (back to the groups, up to Party).
    m.current_hp = 50;
    h.g().party.characters[0].status = 'P';
    h.key('\b');
    h.ball(RawInputKind::TrackballUp);
    h.key('\r');
    h.ball(RawInputKind::TrackballDown);
    h.key('\r'); // Heal Party
    const int healed = m.current_hp;
    h.ball(RawInputKind::TrackballDown);
    h.key('\r'); // Cure Party
    check(healed == 300 && m.status == 'G', "C5", "Heal Party and Cure Party: HP 50 -> 300, poison cured");
    h.key('\b'); // back to the groups, on Party
    const bool on_party = std::string(h.rt->system_menu_view().title) == "Cheats" && h.rt->system_menu_view().selected_line == 0;
    h.key('\b'); // back to the root, on Cheats
    check(on_party && h.rt->system_menu_view().selected_line == 5, "C6",
          "Back returns to the groups on Party, then to the root with Cheats selected");
    h.key('m', true);
    // God Mode in play: a poisoned walk costs nothing.
    m.status = 'P';
    const int hp = m.current_hp;
    for (int i = 0; i < 6; ++i) h.ball(i % 2 ? RawInputKind::TrackballLeft : RawInputKind::TrackballRight);
    check(m.current_hp == hp && m.status == 'P', "C7", "God Mode: six poisoned steps, no HP lost");
    // Save: the sidecar carries it; turn God Mode off; the load brings it back.
    const uint32_t used = h.g().enhanced.cheats_used;
    h.key('s', true);
    const bool side = newest_sidecar();
    const auto &e = g_side["enhanced"];
    check(side && e["godMode"].truth() && e["cheatsUsed"].integer() == int64_t(used) &&
              used == (openu5::cheat_bit(openu5::CheatKind::GodMode) | openu5::cheat_bit(openu5::CheatKind::HealParty) |
                       openu5::cheat_bit(openu5::CheatKind::CureParty) | openu5::cheat_bit(openu5::CheatKind::AddGold) |
                       openu5::cheat_bit(openu5::CheatKind::MaxGold)),
          "C8", "Alt+S: the save's sidecar holds \"enhanced\" {godMode: true, cheatsUsed: all five}");
    open_cheats(h);
    h.key('\r');
    const bool off = !h.g().enhanced.god_mode;
    h.key('m', true);
    h.key('l', true);
    check(off && h.g().enhanced.god_mode && h.g().enhanced.cheats_used == used, "C9",
          "God Mode off, then Alt+L: the saved journey's God Mode and cheats-used bits come back");
    // A journey that never cheated saves no trace of any of it.
    Run plain;
    plain.key('s', true);
    check(newest_sidecar() && !g_side.has("enhanced"), "C10", "a journey with no cheat: no \"enhanced\" key in its save");
    // In combat Heal and Cure are refused (the arena holds its own HP copies).
    {
        auto &ctx = plain.rt->command_context_for_test();
        int tile = -1;
        for (size_t i = 0; i < ctx.outdoor->resources->enemy_count; ++i)
            if (const auto *d = ctx.outdoor->resources->enemies[i]; d && d->index == 41) tile = d->tile;
        ctx.outdoor->enemies.clear();
        openu5::OutdoorEnemy troll{};
        troll.definition = 41;
        troll.tile = tile;
        troll.x = plain.g().position.xy.x + 1;
        troll.y = plain.g().position.xy.y;
        ctx.outdoor->enemies.push_back(troll);
        plain.key(' ');
        plain.frames(300);
        const bool fighting = ctx.combat && plain.rt->combat_state().initialized;
        plain.g().party.characters[0].current_hp = 40;
        open_cheats(plain, 1);
        plain.key('\r');
        const std::string footer = plain.rt->system_menu_view().footer;
        check(fighting && footer == "Not during combat" && plain.g().party.characters[0].current_hp == 40 &&
                  plain.g().enhanced.cheats_used == 0,
              "C11", "in combat Heal Party is refused (\"" + footer + "\"), nothing changes, nothing is marked");
        plain.key('m', true);
    }
}

void test_difficulty() {
    Run h;
    auto &ally = h.g().party.characters[0];
    // R1. System Menu > Difficulty: the presets, the journey's own first.
    // A4-ENH2 (changed on purpose): a fourth row, Custom, and under the rows
    // the selected one's actual values (a4_enh2_runtime U1 checks them); the
    // footer no longer abbreviates them.
    h.key('m', true);
    for (int i = 0; i < 4; ++i) h.ball(RawInputKind::TrackballDown);
    const std::string root_footer = h.rt->system_menu_view().footer;
    h.key('\r');
    auto v = h.rt->system_menu_view();
    const std::string f0 = v.footer;
    h.ball(RawInputKind::TrackballDown);
    const std::string f1 = h.rt->system_menu_view().footer;
    h.ball(RawInputKind::TrackballDown);
    const std::string f2 = h.rt->system_menu_view().footer;
    check(root_footer == "Original, or a gentler journey" && std::string(v.title) == "Difficulty" &&
              v.line_count == 4 + size_t(openu5::RuleField::Count) && std::string(v.lines[0]) == "Original" &&
              std::string(v.lines[1]) == "Relaxed" && std::string(v.lines[2]) == "Easy" &&
              std::string(v.lines[3]) == "Custom" && v.selected_line == 0 &&
              std::string(v.subtitle) == "Now: Original (kept with this journey)" &&
              f0 == "The 1988 rules, unchanged" && f1 == "Enter: use these rules" && f2 == "Enter: use these rules",
          "R1", "the Difficulty page: Original first and current, Custom last; footers:\n  " + f1 +
                    "\n  " + f2);
    // R2. Enter on Easy: the journey's preset, said in the footer and the transcript.
    h.set_mark();
    h.key('\r');
    v = h.rt->system_menu_view();
    check(h.g().enhanced.difficulty == openu5::Difficulty::Easy && std::string(v.footer) == "Difficulty: Easy" &&
              std::string(v.subtitle) == "Now: Easy (kept with this journey)" && h.count_since("Difficulty: Easy") == 1 &&
              h.g().enhanced.cheats_used == 0,
          "R2", "Enter: Easy now, in the footer and the transcript; a difficulty is not a cheat (nothing marked)");
    h.key('\b');
    check(h.rt->system_menu_view().selected_line == 4, "R3", "Back returns to the root with Difficulty selected");
    h.key('m', true);
    // R4. In play: ten poisoned steps cost one HP on Easy (every 10th turn).
    ally.status = 'P';
    const int hp = ally.current_hp;
    const int64_t turns = h.g().turns_since_start;
    for (int i = 0; i < 10; ++i) h.ball(i % 2 ? RawInputKind::TrackballLeft : RawInputKind::TrackballRight);
    const int64_t walked = h.g().turns_since_start - turns;
    check(walked == 10 && hp - ally.current_hp == 1, "R4",
          "Easy: " + std::to_string(walked) + " poisoned turns cost " + std::to_string(hp - ally.current_hp) +
              " HP (Original: one a turn)");
    // R5. Saved with the journey; Alt+L after switching back brings Easy back.
    h.key('s', true);
    const bool side = newest_sidecar();
    const bool easy_key = g_side["enhanced"]["difficulty"].string == openu5::save::Json("easy").string &&
                          !g_side["enhanced"]["godMode"].truth();
    h.key('m', true);
    for (int i = 0; i < 4; ++i) h.ball(RawInputKind::TrackballDown);
    h.key('\r');
    h.ball(RawInputKind::TrackballUp);
    h.ball(RawInputKind::TrackballUp);
    h.key('\r'); // Original
    const bool back_to_original = h.g().enhanced.difficulty == openu5::Difficulty::Original;
    h.key('m', true);
    h.key('l', true);
    check(side && easy_key && back_to_original && h.g().enhanced.difficulty == openu5::Difficulty::Easy, "R5",
          "Alt+S keeps \"difficulty\": \"easy\" in the sidecar; Original, then Alt+L: Easy again");
}

// R6's circuit: the first 15 x 15 square whose edge is open ground (as the
// preservation golden's walk), entered at midnight.
openu5::Position open_square(const openu5::WorldData &w) {
    auto ground = [&](int x, int y) {
        const int t = w.overworld[size_t(y) * 256 + size_t(x)];
        return t == 5 || t == 6 || (t >= 9 && t <= 15);
    };
    for (int y = 40; y < 220; ++y)
        for (int x = 40; x < 220; ++x) {
            bool ok = true;
            for (int i = 0; ok && i < 14; ++i)
                ok = ground(x + i, y) && ground(x + 14, y + i) && ground(x + 14 - i, y + 14) && ground(x, y + 14 - i);
            if (ok) return {uint8_t(x), uint8_t(y)};
        }
    return {94, 108};
}

void test_encounters() {
    // R6. The same night walk, Original and Easy side by side. They match
    // step for step until the first turn whose passed spawn roll Easy turns
    // down: there Original gains a monster and Easy does not -- the live
    // world() turn consults the encounter rule (no RNG draw of its own).
    Run o, e;
    for (Run *r : {&o, &e}) {
        auto &g = r->g();
        g.position.xy = open_square(pack->world);
        g.time.hour = 0;
        g.time.minute = 0;
        g.rng.seed(0x5eed);
        g.enhanced.difficulty = r == &e ? openu5::Difficulty::Easy : openu5::Difficulty::Original;
        g.party.characters[0].current_hp = g.party.characters[0].max_hp = 999;
    }
    static constexpr RawInputKind legs[] = {RawInputKind::TrackballRight, RawInputKind::TrackballDown,
                                            RawInputKind::TrackballLeft, RawInputKind::TrackballUp};
    int split = -1, spawns_o = 0, spawns_e = 0;
    bool refused_at_split = false, same_before = true;
    for (int step = 0; step < 200; ++step) {
        const size_t before_o = o.rt->command_context_for_test().outdoor->enemies.size();
        const size_t before_e = e.rt->command_context_for_test().outdoor->enemies.size();
        o.ball(legs[(step / 14) % 4]);
        e.ball(legs[(step / 14) % 4]);
        const size_t after_o = o.rt->command_context_for_test().outdoor->enemies.size();
        const size_t after_e = e.rt->command_context_for_test().outdoor->enemies.size();
        spawns_o += after_o > before_o;
        spawns_e += after_e > before_e;
        if (split < 0 && (o.g().rng.get_seed() != e.g().rng.get_seed() || after_o != after_e)) {
            split = step;
            // world() runs after the turn's housekeeping counted it.
            openu5::GameState probe = e.g();
            refused_at_split = !openu5::rules_encounter_allowed(probe);
        }
        if (split < 0)
            same_before = same_before && o.g().position.xy.x == e.g().position.xy.x &&
                          o.g().position.xy.y == e.g().position.xy.y && o.g().turns_since_start == e.g().turns_since_start;
        if (o.mode() == UiMode::Combat || e.mode() == UiMode::Combat) break; // a fight ends the comparison
    }
    check(split >= 0 && same_before && refused_at_split, "R6",
          "Original and Easy walk the same night circuit identically until step " + std::to_string(split) +
              ", the first turn Easy's encounter rule refuses: there Original's spawner draws and Easy's does "
              "not (monsters added so far " + std::to_string(spawns_o) + " vs " + std::to_string(spawns_e) + ")");
}

bool lists(const openu5::FrontendView &v, const char *needle) {
    for (size_t i = 0; i < v.line_count; ++i)
        if (v.lines[i] && std::strstr(v.lines[i], needle)) return true;
    return false;
}

void test_developer() {
    // A card whose settings.json still says developerToolsVisible: true (the
    // old Settings row's switch): nothing shows a Developer row any more.
    tdeck::a3_host_settings_enabled() = true;
    openu5::FrontendSettings visible{};
    visible.developer_tools_visible = true;
    visible.trackball_speed = openu5::kTrackballSpeedLegacy;
    encode_settings(visible, tdeck::a3_host_settings_text());
    Run h;
    h.key('m', true);
    const auto root = h.rt->system_menu_view();
    for (int i = 0; i < 3; ++i) h.ball(RawInputKind::TrackballDown);
    h.key('\r');
    const auto settings = h.rt->system_menu_view();
    check(h.rt->device_settings().developer_tools_visible && !lists(root, "Developer") && !lists(settings, "Developer") &&
              settings.line_count == 7, // A4-POLISH3 appended Keyboard Backlight
          "D1", "System Menu: no Developer row on the root or in Settings, even with developerToolsVisible on");
    h.key('m', true);
    h.key('d', true);
    check(h.mode() == UiMode::DebugMenu, "D2", "Alt+D in the game still opens the Developer menu");
    h.key('\b');
    check(h.mode() != UiMode::DebugMenu, "D3", "and Back closes it");
    // The title: no Developer row, no 'D' hotkey; Alt+D opens the tools.
    h.key('m', true);
    for (int i = 0; i < 12 && h.rt->system_menu_view().selected_line + 1 != int(h.rt->system_menu_view().line_count); ++i)
        h.ball(RawInputKind::TrackballDown);
    h.key('\r');
    h.key(' ');
    const auto menu = h.rt->frontend_view();
    h.key('d');
    const bool d_inert = h.rt->frontend_open() && h.rt->frontend_state() == openu5::FrontendState::MainMenu;
    check(menu.line_count == 8 && !lists(menu, "Developer") && std::string(menu.footer).find(" D") == std::string::npos &&
              d_inert,
          "D4", "the title's main menu: eight rows, no Developer, no D hotkey (D does nothing)");
    h.key('d', true);
    check(!h.rt->frontend_open() && h.mode() == UiMode::DebugMenu, "D5",
          "Alt+D on the title opens the Developer menu, as the hidden row did");
    tdeck::a3_host_settings_enabled() = false;
    tdeck::a3_host_settings_text().clear();
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    tdeck::AlphaResourcePack source;
    tdeck::AlphaResourceReport report_out{};
    if (source.open(argv[1], report_out) != ESP_OK) return 2;
    static tdeck::AlphaResourceOwners owners{};
    if (source.load(owners, report_out) != ESP_OK) return 2;
    pack = &owners;

    test_click();
    test_report();
    test_speed();
    test_developer();
    test_cheats();
    test_difficulty();
    test_encounters();

    std::printf("\nA4-ENH1 runtime: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
