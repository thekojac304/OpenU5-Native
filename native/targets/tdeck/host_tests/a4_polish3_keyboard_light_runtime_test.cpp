// Alpha 4 A4-POLISH3 (ALPHA4_UI.md section 13) -- the keyboard backlight on
// the REAL AlphaRuntime (A3-HF4's source set: the capture Board, the memory
// card, the virtual clock). The device binds InputHardware's
// set_keyboard_backlight(); here a recorder stands in for it.
//
//   a4_polish3_keyboard_light_runtime <openu5-alpha1-resources.bin>
//
//   B  boot: the saved level is sent once; an older card sends Off
//   K  System Menu > Settings > Keyboard Backlight: each change is sent at
//      once, nothing else sends anything, Mic saves it to settings.json
#include "../main/alpha_runtime.h"
#include "../main/tdeck_board.h"
#include "esp_timer.h"
#include "openu5/frontend_settings.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

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

std::vector<int> sent;
void record(void *, uint8_t level) { sent.push_back(level); }
std::string list(const std::vector<int> &v) {
    std::string s;
    for (int x : v) s += std::to_string(x) + ",";
    return s.empty() ? "none" : s;
}

struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    explicit Run(bool available = true) {
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
        rt->attach_host_test_fixture(f); // loads settings.json, as initialize() does
        auto &g = rt->game();
        g.position.map = {0, 0};
        g.position.xy = {pack->location_x[kCastle - 1], pack->location_y[kCastle - 1]};
        g.position.xy.y = uint8_t(g.position.xy.y + 2);
        g.time.hour = 12;
        g.party.character_count = g.party.party_size = 1;
        g.party.active_character = 255;
        auto &m = g.party.characters[0];
        std::snprintf(m.name, sizeof(m.name), "Avatar");
        m.status = 'G';
        m.current_hp = m.max_hp = 300;
        sent.clear();
        // main.cpp binds the keyboard after initialize(), as here.
        rt->attach_keyboard_light({nullptr, record, available});
        frames(3);
    }
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
    void ball(RawInputKind k) {
        openu5_host_virtual_clock_us() += 100000;
        tdeck::RawInputEvent e{};
        e.kind = k;
        raw(e);
    }
    void mic() {
        tdeck::RawInputEvent e{};
        e.kind = RawInputKind::Keyboard;
        e.column = tdeck::kMicrophoneKeyColumn;
        e.row = tdeck::kMicrophoneKeyRow;
        e.transition = tdeck::KeyTransition::Pressed;
        openu5_host_virtual_clock_us() += 100000;
        raw(e);
        e.transition = tdeck::KeyTransition::Released;
        openu5_host_virtual_clock_us() += 100000;
        raw(e);
    }
    /** System Menu > Settings, cursor on Keyboard Backlight. */
    void open_row() {
        key('m', true);
        for (int i = 0; i < 3; ++i) ball(RawInputKind::TrackballDown);
        key('\r');
        for (int i = 0; i < 6; ++i) ball(RawInputKind::TrackballDown);
    }
    std::string row() { return rt->system_menu_view().lines[6] ? rt->system_menu_view().lines[6] : ""; }
    std::string footer() { return rt->system_menu_view().footer ? rt->system_menu_view().footer : ""; }
};

void test_boot() {
    tdeck::a3_host_settings_enabled() = true;
    FrontendSettings saved{};
    saved.keyboard_backlight = 3;
    encode_settings(saved, tdeck::a3_host_settings_text());
    {
        Run h;
        const auto at_boot = sent;
        h.frames(50);
        check(at_boot == std::vector<int>{3} && sent == at_boot && h.rt->device_settings().keyboard_backlight == 3, "B1",
              "a saved High is sent once when the keyboard is bound, and frames send nothing more: " + list(sent));
    }
    tdeck::a3_host_settings_text() =
        "{\"version\":1,\"brightness\":60,\"movementMode\":false,\"trackballResponsiveness\":100,\"uiSize\":1,"
        "\"developerToolsVisible\":false,\"soundVolume\":40,\"musicVolume\":70,\"touchControls\":false,"
        "\"trackballSpeed\":10}";
    {
        Run h;
        check(sent == std::vector<int>{0} && h.rt->device_settings().brightness == 60 &&
                  h.rt->device_settings().sound_volume == 40,
              "B2", "an older settings.json (no key) loads its values and sends Off -- the keyboard's own boot state: " +
                        list(sent));
    }
    tdeck::a3_host_settings_text().clear();
    {
        Run h;
        check(sent == std::vector<int>{0}, "B3", "no settings.json at all: the default, Off, sent once: " + list(sent));
    }
}

void test_settings_row() {
    tdeck::a3_host_settings_enabled() = true;
    tdeck::a3_host_settings_text().clear();
    Run h;
    sent.clear();
    h.open_row();
    const bool quiet_navigation = sent.empty();
    const std::string off = h.row();
    h.ball(RawInputKind::TrackballRight);
    const auto after_one = sent;
    const std::string low = h.row();
    h.frames(40);
    const bool held = sent == after_one;
    check(quiet_navigation && off == "Keyboard Backlight: Off" && low == "Keyboard Backlight: Low" &&
              after_one == std::vector<int>{1} && held,
          "K1", "opening the menu and moving the cursor sends nothing; Right sends Low at once, before any save, "
                "and only once: " + list(sent));
    h.ball(RawInputKind::TrackballRight);
    h.ball(RawInputKind::TrackballRight);
    h.ball(RawInputKind::TrackballRight);
    check(sent == std::vector<int>{1, 2, 3, 4} && h.row() == "Keyboard Backlight: Max" &&
              h.footer() == "Left/right: Off to Max; Mic saves",
          "K2", "Medium, High, Max each sent as chosen: " + list(sent));
    const uint8_t brightness = h.rt->device_settings().brightness;
    h.ball(RawInputKind::TrackballUp); // Music Volume (unavailable: nothing changes)
    h.ball(RawInputKind::TrackballRight);
    h.ball(RawInputKind::TrackballDown);
    check(sent.size() == 4 && h.rt->device_settings().brightness == brightness, "K3",
          "an edit that changes nothing else and the moves around it send no keyboard write; display brightness unchanged");
    const std::string before_save = tdeck::a3_host_settings_text();
    h.mic();
    const std::string saved = tdeck::a3_host_settings_text();
    check(before_save.empty() && saved.find("\"keyboardBacklight\":4") != std::string::npos && sent.size() == 4,
          "K4", "Mic leaves Settings and writes \"keyboardBacklight\":4 to settings.json; the save sends nothing again");
    // A reboot on that card: the saved Max is applied at boot.
    {
        Run again;
        check(sent == std::vector<int>{4}, "K5", "after a reboot the saved Max is applied once: " + list(sent));
    }
}

void test_wrap_and_absent() {
    tdeck::a3_host_settings_enabled() = true;
    tdeck::a3_host_settings_text().clear();
    {
        Run h;
        sent.clear();
        h.open_row();
        h.ball(RawInputKind::TrackballLeft);
        h.ball(RawInputKind::TrackballRight);
        check(sent == std::vector<int>{4, 0} && h.row() == "Keyboard Backlight: Off", "W1",
              "Left from Off wraps to Max, Right from Max back to Off, each sent: " + list(sent));
    }
    {
        Run h(false);
        const auto boot = sent;
        h.open_row();
        h.ball(RawInputKind::TrackballRight);
        check(boot == std::vector<int>{0} && h.row() == "Keyboard Backlight: Low" &&
                  h.footer() == "No keyboard found; saved for next boot" && h.rt->device_settings().keyboard_backlight == 1,
              "W2", "no keyboard at boot: the row still edits and is kept for the next boot, and the footer says so");
    }
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

    test_boot();
    test_settings_row();
    test_wrap_and_absent();

    std::printf("\nA4-POLISH3 keyboard light runtime: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
