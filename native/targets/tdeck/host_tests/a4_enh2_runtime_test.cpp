// Alpha 4 A4-ENH2 (ALPHA4_UI.md section 11) -- the extended difficulty and
// cheats on the REAL AlphaRuntime (A3-HF4's source set: the capture Board, the
// memory card, the virtual clock), through the device's own System Menu.
//
//   a4_enh2_runtime <openu5-alpha1-resources.bin>
//
//   U  the Difficulty page (four rows, the selected row's actual values) and
//      the Custom page: Enter on Custom, left/right edits applied at once,
//      Back, switching presets and back to Custom, the values in the save,
//      Alt+L, and a "power cycle" (a fresh runtime over the same card); in
//      play: Custom Starvation Off on a starving walk, the dungeon wanderer's
//      share over real entries into Deceit
//   Y  the cheat groups: Cheats > Party / Inventory through the device's
//      menu, each new cheat, the save's mark, the refusal in a fight
#include "../main/alpha_runtime.h"
#include "../main/tdeck_board.h"
#include "esp_timer.h"
#include "openu5/outdoor.h"
#include "openu5/quest.h"
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
size_t dungeon_count = 0;
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
        f.dungeons = pack->dungeons;
        f.dungeon_count = dungeon_count;
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
                          h.line(10) == "  Dungeon encounters: 100%" && h.line(11) == "  Starvation: Original" &&
                          h.footer() == "The 1988 rules, unchanged";
    h.ball(RawInputKind::TrackballDown, 2);
    std::string easy;
    for (int i = 4; i < int(h.menu().line_count); ++i) easy += h.line(i) + "\n";
    check(rows && original && h.menu().selected_line == 2 && h.line(4) == "  Enemy damage: 65%" && h.line(5) == "  Player damage: 120%" &&
              h.line(6) == "  XP rate: 2.0x" && h.line(7) == "  Overworld encounters: 65%" && h.line(8) == "  Poison: Light" &&
              h.line(9) == "  Hunger: 50%" && h.line(10) == "  Dungeon encounters: 65%" && h.line(11) == "  Starvation: Minimal" &&
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
    h.ball(RawInputKind::TrackballDown);
    h.ball(RawInputKind::TrackballLeft, 9);                                    // dungeon encounters held at 25 %
    h.ball(RawInputKind::TrackballDown);
    h.ball(RawInputKind::TrackballLeft);                                       // starvation Original -> Reduced
    const GameplayRules want{75, 120, 300, 65, 10, 0, 25, 50};
    check(rules_equal(h.g().enhanced.custom, want) && h.line(0) == "Enemy damage: 75%" && h.line(1) == "Player damage: 120%" &&
              h.line(2) == "XP rate: 3.0x" && h.line(3) == "Overworld encounters: 65%" && h.line(4) == "Poison: Light" &&
              h.line(5) == "Hunger: Off" && h.line(6) == "Dungeon encounters: 25%" && h.line(7) == "Starvation: Reduced" &&
              h.count_since("Difficulty") == 0 && h.g().enhanced.cheats_used == 0,
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
                      e["custom"].at(0).integer() == 75 && e["custom"].at(5).integer() == 0 &&
                      e["custom"].at(6).integer() == 25 && e["custom"].at(7).integer() == 50 && !e.has("toggles");
    h.g().enhanced.difficulty = Difficulty::Original;
    h.g().enhanced.custom = kOriginalRules;
    h.key('l', true);
    check(side && keys && h.g().enhanced.difficulty == Difficulty::Custom && rules_equal(h.g().enhanced.custom, want), "U6",
          "Alt+S writes \"difficulty\":\"custom\" and the eight values; Alt+L brings them back");
    // U7. Power cycle: a fresh runtime over the same card, then the load.
    Run cold(false);
    const bool fresh = cold.g().enhanced.difficulty == Difficulty::Original;
    cold.key('l', true);
    check(fresh && cold.g().enhanced.difficulty == Difficulty::Custom && rules_equal(cold.g().enhanced.custom, want), "U7",
          "after a power cycle (a fresh runtime, the same card) the load restores Custom and its values");
}
// The device's own dungeon entry: stand on Deceit's entrance (its Word of
// Passage granted) and (E)nter; (K)limb at the entry ladder leaves again.
bool enter_deceit(Run &h) {
    h.g().position.map = {0, 0};
    h.g().position.xy = {pack->location_x[33 - 1], pack->location_y[33 - 1]};
    set_quest_flag(h.g().quest, QuestFlag::Word33);
    h.key('e');
    h.frames(20);
    return h.rt->dungeon_state().active;
}

void test_dungeon_and_starvation() {
    // U8. Starvation in play: food 0, two hours of walking on the overworld.
    // Original says "Starving!" and takes HP; Custom Starvation Off neither.
    struct Walk { int lost = 0, lines = 0; };
    auto walk = [](bool off) {
        Walk w;
        Run h;
        h.g().food = 0;
        h.g().time.hour = 9;
        h.g().time.minute = 50;
        if (off) {
            h.g().enhanced.difficulty = Difficulty::Custom;
            h.g().enhanced.custom.starvation_pct = 0;
        }
        const int hp = h.g().party.characters[0].current_hp;
        h.set_mark();
        for (int i = 0; i < 70; ++i) h.ball(i % 2 ? RawInputKind::TrackballLeft : RawInputKind::TrackballRight);
        w.lost = hp - h.g().party.characters[0].current_hp;
        w.lines = h.count_since("Starving");
        return w;
    };
    const Walk orig = walk(false), off = walk(true);
    check(orig.lost > 0 && orig.lines >= 2 && off.lost == 0 && off.lines == 0, "U8",
          "starving across two hours on the device: Original " + std::to_string(orig.lines) + " \"Starving!\", " +
              std::to_string(orig.lost) + " HP; Custom Starvation Off: none, 0 HP");
    // U9. Dungeon wanderers in play: 24 real entries into Deceit (each a
    // re-arm), a few turns between them. Original places one whenever its
    // eight tries find a free cell on Deceit's first floor (most visits);
    // Custom 25 % keeps about a quarter of those.
    auto entries = [](uint16_t pct) {
        Run h;
        h.g().enhanced.difficulty = pct == 100 ? Difficulty::Original : Difficulty::Custom;
        h.g().enhanced.custom.dungeon_encounter_pct = pct;
        int placed = 0, entered = 0;
        for (int i = 0; i < 24; ++i) {
            h.g().turns_since_start += 7; // a few overworld turns between visits
            if (!enter_deceit(h)) continue;
            ++entered;
            placed += h.rt->dungeon_state().wanderer.type != 255;
            h.key('k'); // the entry ladder leads back up
            h.frames(20);
        }
        return std::make_pair(entered, placed);
    };
    const auto o = entries(100), c = entries(25);
    check(o.first == 24 && c.first == 24 && o.second >= 10 && c.second < o.second / 2, "U9",
          "24 entries into Deceit (" + std::to_string(o.first) + " / " + std::to_string(c.first) +
              " entered): Original places the wanderer " + std::to_string(o.second) + " times, Custom 25 % " +
              std::to_string(c.second));
}
/** System Menu > Cheats (row 5) > group `group`, then `row` Downs. */
void open_cheat(Run &h, int group, int row) {
    h.key('m', true);
    h.ball(RawInputKind::TrackballDown, 5);
    h.key('\r');
    h.ball(RawInputKind::TrackballDown, group);
    h.key('\r');
    h.ball(RawInputKind::TrackballDown, row);
}

void test_cheat_groups() {
    Run h;
    auto &g = h.g();
    g.party.character_count = g.party.party_size = 2;
    auto &ally = g.party.characters[1];
    ally = g.party.characters[0];
    std::snprintf(ally.name, sizeof(ally.name), "Shamino");
    ally.character_class = 'B';
    ally.intelligence = 24;
    ally.status = 'D';
    ally.current_hp = 0;
    ally.max_hp = 240;
    ally.current_mp = 0;
    g.party.characters[0].current_mp = 0;
    // Y1. Cheats lists its groups; each says what it holds.
    h.key('m', true);
    h.ball(RawInputKind::TrackballDown, 5);
    h.key('\r');
    auto v = h.menu();
    const std::string f0 = h.footer();
    h.ball(RawInputKind::TrackballDown);
    const std::string f1 = h.footer();
    check(std::string(v.title) == "Cheats" && v.line_count >= 2 && h.line(0) == "Party" && h.line(1) == "Inventory" &&
              f0 == "God Mode, heal, cure, magic, revive" && f1 == "Gold, food, keys, torches, gems, reagents" &&
              std::string(v.subtitle) == "Using one marks this journey's save",
          "Y1", "Cheats: the groups (Party, Inventory, ...), each footer naming its cheats");
    h.key('m', true);
    // Y2. Party > Restore MP and Revive Party through the device's menu.
    open_cheat(h, 0, 3);
    h.set_mark();
    h.key('\r');
    const bool mp = g.party.characters[0].current_mp == 30 && ally.current_mp == 0 && h.footer() == "MP restored: 1" &&
                    h.count_since("MP restored: 1") == 1;
    h.ball(RawInputKind::TrackballDown);
    h.key('\r');
    const bool revived = ally.status == 'G' && ally.current_hp == 240 && ally.current_mp == 12 && h.footer() == "Revived: 1";
    check(h.line(3) == "Restore MP" && h.line(4) == "Revive Party" && mp && revived, "Y2",
          "Party > Restore MP: the Avatar's 0 -> 30 (the dead bard has none yet); Revive Party: the bard 'G', 240 HP, 12 MP");
    // Y3. Inventory: every Max cheat and the reagents, from the device.
    h.key('\b');
    const bool back_on_party = std::string(h.menu().title) == "Cheats" && h.menu().selected_line == 0;
    h.ball(RawInputKind::TrackballDown);
    h.key('\r');
    g.food = 50;
    g.keys = g.torches = g.gems = 1;
    for (auto &q : g.reagent_quantities) q = 3;
    std::string footers;
    for (int row = 2; row <= 6; ++row) {
        while (h.menu().selected_line != row) h.ball(RawInputKind::TrackballDown);
        h.key('\r');
        footers += h.footer() + "|";
    }
    bool reagents = true;
    for (auto q : g.reagent_quantities) reagents = reagents && q == 99;
    check(back_on_party && std::string(h.menu().title) == "Inventory" && g.food == 9999 && g.keys == 99 && g.torches == 99 &&
              g.gems == 99 && reagents && footers == "Food: 9999|Keys: 99|Torches: 99|Gems: 99|Reagents: 99 each|",
          "Y3", "Inventory > Max Food / Keys / Torches / Gems / Give Reagents from the device: " + footers);
    // Y4. Every applied cheat is marked in the save, and a load keeps them.
    h.key('m', true);
    h.key('s', true);
    const uint32_t want = cheat_bit(CheatKind::RestoreMp) | cheat_bit(CheatKind::ReviveParty) | cheat_bit(CheatKind::MaxFood) |
                          cheat_bit(CheatKind::MaxKeys) | cheat_bit(CheatKind::MaxTorches) | cheat_bit(CheatKind::MaxGems) |
                          cheat_bit(CheatKind::GiveReagents);
    const bool side = newest_sidecar() && g_side["enhanced"]["cheatsUsed"].integer() == int64_t(want);
    g.food = 10;
    h.key('l', true);
    check(side && h.g().enhanced.cheats_used == want && h.g().food == 9999, "Y4",
          "Alt+S: \"cheatsUsed\" holds the seven new bits; Alt+L brings back the journey and its mark");
    // Y5. Revive in a fight is refused (the arena seats no dead member).
    Run f;
    auto &fg = f.g();
    fg.party.character_count = fg.party.party_size = 2;
    fg.party.characters[1] = fg.party.characters[0];
    fg.party.characters[1].status = 'D';
    fg.party.characters[1].current_hp = 0;
    auto &ctx = f.rt->command_context_for_test();
    int tile = -1;
    for (size_t i = 0; i < ctx.outdoor->resources->enemy_count; ++i)
        if (const auto *d = ctx.outdoor->resources->enemies[i]; d && d->index == 41) tile = d->tile;
    ctx.outdoor->enemies.clear();
    OutdoorEnemy troll{};
    troll.definition = 41;
    troll.tile = tile;
    troll.x = fg.position.xy.x + 1;
    troll.y = fg.position.xy.y;
    ctx.outdoor->enemies.push_back(troll);
    f.key(' ');
    f.frames(300);
    const bool fighting = ctx.combat && f.rt->combat_state().initialized;
    open_cheat(f, 0, 4);
    f.key('\r');
    check(fighting && f.footer() == "Not during combat" && fg.party.characters[1].status == 'D' && fg.enhanced.cheats_used == 0,
          "Y5", "Revive Party during a fight: \"Not during combat\", the dead stay dead, nothing marked");
    f.key('m', true);
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
    dungeon_count = report_out.dungeon_count;

    test_difficulty_page();
    test_dungeon_and_starvation();
    test_cheat_groups();

    std::printf("\nA4-ENH2 runtime: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
