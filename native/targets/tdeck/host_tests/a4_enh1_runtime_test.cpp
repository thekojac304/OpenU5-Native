// Alpha 4 A4-ENH1 (ALPHA4_UI.md section 10) -- the enhancement pass on the
// REAL AlphaRuntime (A3-HF4's source set: the capture Board, the memory card,
// the virtual clock).
//
//   a4_enh1_runtime <openu5-alpha1-resources.bin>
//
//   W  the trackball click: an immediate one-press WASD toggle on every
//      screen, with its transcript line, and never a key for what is open
//   T  Developer > Diagnostics > "Trackball stats (live)"
#include "../main/alpha_runtime.h"
#include "../main/tdeck_board.h"
#include "esp_timer.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

using namespace openu5;
using tdeck::RawInputKind;

namespace tdeck {
void host_memory_save_forget_for_test();
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

    std::printf("\nA4-ENH1 runtime: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
