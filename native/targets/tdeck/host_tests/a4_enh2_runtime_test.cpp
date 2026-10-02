// Alpha 4 A4-ENH2 (ALPHA4_UI.md section 11) -- the extended difficulty and
// cheats on the REAL AlphaRuntime (A3-HF4's source set: the capture Board, the
// memory card, the virtual clock), through the device's own System Menu.
//
//   a4_enh2_runtime <openu5-alpha1-resources.bin>
//
//   U  the Difficulty page (four rows, the selected row's actual values) and
//      the Custom page: Enter on Custom, left/right edits applied at once,
//      Back, switching presets and back to Custom, the values in the save,
//      Alt+L, and a "power cycle" (a fresh runtime over the same card)
#include "../main/alpha_runtime.h"
#include "../main/tdeck_board.h"
#include "esp_timer.h"
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
    // `wipe` false keeps the memory card: a fresh runtime over the same card
    // is the host's power cycle.
    explicit Run(bool wipe = true) {
        openu5_host_virtual_clock_us() = 5'000'000;
        batch37_reset_screen();
        if (wipe) tdeck::host_memory_save_forget_for_test();
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
    void ball(RawInputKind k, int n = 1) {
        for (int i = 0; i < n; ++i) {
            openu5_host_virtual_clock_us() += 100000;
            tdeck::RawInputEvent e{};
            e.kind = k;
            raw(e);
        }
    }
    void set_mark() { mark = ui().transcript_size(); }
    int count_since(const char *needle) {
        int n = 0;
        for (size_t i = mark; i < ui().transcript_size(); ++i)
            if (const auto *b = ui().transcript_at(i); b && std::strstr(b->text, needle)) ++n;
        return n;
    }
    FrontendView menu() { return rt->system_menu_view(); }
    std::string footer() { return menu().footer ? menu().footer : ""; }
    std::string line(int i) { const auto v = menu(); return i < int(v.line_count) && v.lines[i] ? v.lines[i] : ""; }
};

// The newest save's sidecar gameState, as written to the (memory) card.
openu5::save::Json g_side;
bool newest_sidecar() {
    g_side = openu5::save::Json{};
    return tdeck::host_memory_save_edit_for_test(true, [](openu5::save::Json &gs) { g_side = gs; });
}

/** System Menu > Difficulty (row 4: after Settings) -- the page opens on the
 *  journey's own difficulty -- then `row` Downs. */
void open_difficulty(Run &h, int row = 0) {
    h.key('m', true);
    h.ball(RawInputKind::TrackballDown, 4);
    h.key('\r');
    h.ball(RawInputKind::TrackballDown, row);
}

bool rules_equal(const GameplayRules &a, const GameplayRules &b) { return std::memcmp(&a, &b, sizeof a) == 0; }

void test_difficulty_page() {
    Run h;
    // U1. Four rows; under them, the selected row's actual values.
    open_difficulty(h);
    auto v = h.menu();
    const bool rows = std::string(v.title) == "Difficulty" && v.line_count == 4 + size_t(RuleField::Count) &&
                      h.line(0) == "Original" && h.line(1) == "Relaxed" && h.line(2) == "Easy" && h.line(3) == "Custom" &&
                      v.selected_line == 0 && std::string(v.subtitle) == "Now: Original (kept with this journey)";
    const bool original = h.line(4) == "  Enemy damage: 100%" && h.line(5) == "  Player damage: 100%" &&
                          h.line(6) == "  XP rate: 1.0x" && h.line(7) == "  Overworld encounters: 100%" &&
                          h.line(8) == "  Poison: Original" && h.line(9) == "  Hunger: Original" &&
                          h.footer() == "The 1988 rules, unchanged";
    h.ball(RawInputKind::TrackballDown, 2);
    std::string easy;
    for (int i = 4; i < int(h.menu().line_count); ++i) easy += h.line(i) + "\n";
    check(rows && original && h.menu().selected_line == 2 && h.line(4) == "  Enemy damage: 65%" &&
              h.line(6) == "  XP rate: 2.0x" && h.line(8) == "  Poison: Light" && h.line(9) == "  Hunger: 50%" &&
              h.footer() == "Enter: use these rules",
          "U1", "Difficulty: Original / Relaxed / Easy / Custom, and under them the selected row's values; Easy shows:\n" + easy);
    // U2. Enter on Custom: the journey is Custom (said once), its page opens
    // on Original's values.
    h.ball(RawInputKind::TrackballDown);
    const std::string custom_footer = h.footer();
    h.set_mark();
    h.key('\r');
    v = h.menu();
    check(custom_footer == "Enter: use Custom and set its values" && std::string(v.title) == "Custom Difficulty" &&
              v.line_count == size_t(RuleField::Count) && v.selected_line == 0 && h.line(0) == "Enemy damage: 100%" &&
              h.g().enhanced.difficulty == Difficulty::Custom && h.count_since("Difficulty: Custom") == 1 &&
              rules_equal(h.g().enhanced.custom, kOriginalRules) && std::string(v.subtitle) == "Now: Custom (kept with this journey)",
          "U2", "Enter on Custom: the journey is Custom (one transcript line) and the Custom page opens at Original's values");
    // U3. Left / right edits apply at once, and say nothing in the transcript.
    h.set_mark();
    h.ball(RawInputKind::TrackballLeft, 2);                                    // Enemy damage 100 -> 85 -> 75
    h.ball(RawInputKind::TrackballDown);
    h.ball(RawInputKind::TrackballRight, 2);                                   // Player damage 100 -> 110 -> 120
    h.ball(RawInputKind::TrackballDown);
    h.ball(RawInputKind::TrackballRight, 9);                                   // XP held at 3.0x
    h.ball(RawInputKind::TrackballDown);
    h.ball(RawInputKind::TrackballLeft, 3);                                    // encounters 100 -> 90 -> 75 -> 65
    h.ball(RawInputKind::TrackballDown);
    h.ball(RawInputKind::TrackballLeft, 2);                                    // poison Original -> Reduced -> Light
    h.ball(RawInputKind::TrackballDown);
    h.ball(RawInputKind::TrackballLeft, 9);                                    // hunger held at Off
    const GameplayRules want{75, 120, 300, 65, 10, 0};
    check(rules_equal(h.g().enhanced.custom, want) && h.line(0) == "Enemy damage: 75%" && h.line(1) == "Player damage: 120%" &&
              h.line(2) == "XP rate: 3.0x" && h.line(3) == "Overworld encounters: 65%" && h.line(4) == "Poison: Light" &&
              h.line(5) == "Hunger: Off" && h.count_since("Difficulty") == 0 && h.g().enhanced.cheats_used == 0,
          "U3", "left / right change each value at once (held at the ends), shown on its row; no transcript line, no cheat mark");
    // U4. In play at once: Hunger Off eats nothing across a meal.
    h.key('m', true);
    h.g().time.hour = 11;
    h.g().time.minute = 50;
    const int food = h.g().food;
    for (int i = 0; i < 12; ++i) h.ball(i % 2 ? RawInputKind::TrackballLeft : RawInputKind::TrackballRight);
    check(h.g().time.hour == 12 && h.g().food == food, "U4", "in play: the noon meal under Custom Hunger Off eats nothing");
    // U5. Back from Custom lands on its row; a preset leaves the values
    // alone; Custom again brings them back.
    open_difficulty(h); // the page opens on the journey's own row: Custom
    const bool on_custom = h.menu().selected_line == 3;
    h.key('\r');
    h.key('\b');
    const bool back_row = on_custom && std::string(h.menu().title) == "Difficulty" && h.menu().selected_line == 3;
    h.ball(RawInputKind::TrackballUp);
    h.key('\r');                                                               // Easy
    const bool easy_now = h.g().enhanced.difficulty == Difficulty::Easy && rules_equal(h.g().enhanced.custom, want);
    h.ball(RawInputKind::TrackballUp, 2);
    h.key('\r');                                                               // Original
    const bool orig_now = h.g().enhanced.difficulty == Difficulty::Original && rules_equal(h.g().enhanced.custom, want);
    std::string shown;
    h.ball(RawInputKind::TrackballDown, 3);                                    // Custom's row shows its values
    shown = h.line(4) + "|" + h.line(9);
    h.key('\r');
    check(back_row && easy_now && orig_now && shown == "  Enemy damage: 75%|  Hunger: Off" &&
              h.g().enhanced.difficulty == Difficulty::Custom && rules_equal(h.g().enhanced.custom, want),
          "U5", "Back returns to the Custom row; Easy -> Original -> Custom: the values come back unchanged");
    h.key('\b');
    h.key('\b');
    // U6. The save keeps them; Alt+L restores them after they changed.
    h.key('m', true);
    h.key('s', true);
    const bool side = newest_sidecar();
    const auto &e = g_side["enhanced"];
    const bool keys = e["difficulty"].string == save::Json("custom").string && e["custom"].values.size() == size_t(RuleField::Count) &&
                      e["custom"].at(0).integer() == 75 && e["custom"].at(5).integer() == 0 && !e.has("toggles");
    h.g().enhanced.difficulty = Difficulty::Original;
    h.g().enhanced.custom = kOriginalRules;
    h.key('l', true);
    check(side && keys && h.g().enhanced.difficulty == Difficulty::Custom && rules_equal(h.g().enhanced.custom, want), "U6",
          "Alt+S writes \"difficulty\":\"custom\" and the six values; Alt+L brings them back");
    // U7. Power cycle: a fresh runtime over the same card, then the load.
    Run cold(false);
    const bool fresh = cold.g().enhanced.difficulty == Difficulty::Original;
    cold.key('l', true);
    check(fresh && cold.g().enhanced.difficulty == Difficulty::Custom && rules_equal(cold.g().enhanced.custom, want), "U7",
          "after a power cycle (a fresh runtime, the same card) the load restores Custom and its values");
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

    test_difficulty_page();

    std::printf("\nA4-ENH2 runtime: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
