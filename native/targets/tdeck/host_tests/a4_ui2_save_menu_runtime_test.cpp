// Alpha 4 UI Batch 2 (targets/tdeck/ALPHA4_UI.md section 2.3) -- the save and
// load menus, through the REAL AlphaRuntime on the REAL tdeck_board.cpp over
// the two-slot memory card (the production generation gate).
//
// The card holds two generations of one journey; every save replaces the
// older. Until A4-UI2 the menus listed them by physical slot ("Generation 1:
// Avery", "Generation 2: corrupt") -- which one is newer depended on the save
// count -- and said nothing of where, when, or what a save overwrites.
//
//   M  System Menu: Save Game / Load Game; the selected row says what it does
//      (a save turns the previous save into the backup; Return to Title loses
//      unsaved progress); after a save the footer says what it did
//   L  Load Game: "Latest" and "Backup", ordered by age whichever physical
//      slot holds them, with the leader, the place, and (for the selected row)
//      the date, time and party size; Latest is Continue (it falls back past a
//      damaged latest), Backup loads that generation alone; a damaged or
//      missing backup says so instead of doing nothing silently
//   F  the title: "Create New Character" warns that the current save becomes
//      the backup; Journey Onward names the save Continue will load; its Load
//      Game page lists the same rows and returns to Journey Onward
//   Z  every row fits the Large text size (36 cells), every footer 50
//
//   a4_ui2_save_menu_runtime <openu5-alpha1-resources.bin> [--dump <dir>]
#include "a4_ui2_harness.h"
#include "openu5/debug_developer.h"

namespace tdeck {
void host_memory_save_forget_for_test();
void host_memory_save_damage_for_test();
void host_memory_save_damage_older_for_test();
} // namespace tdeck

using namespace a4_ui2;
using tdeck::host_memory_save_damage_for_test;
using tdeck::host_memory_save_damage_older_for_test;
using tdeck::host_memory_save_forget_for_test;

namespace {
const std::vector<Member> kParty = {{"Avery", 'G', 100}, {"Iolo", 'G', 100}};

std::string line(const FrontendView &v, size_t i) { return i < v.line_count && v.lines[i] ? v.lines[i] : ""; }
std::string footer(const FrontendView &v) { return v.footer ? v.footer : ""; }
std::string lines(const FrontendView &v) {
    std::string out;
    for (size_t i = 0; i < v.line_count; ++i) out += (i ? " | " : "") + line(v, i);
    return out;
}
FrontendView menu(Run &h) { return h.rt->system_menu_view(); }
/** A real game's roster: all 16 records, those past the party marked out of it. */
void full_roster(Run &h) {
    auto &p = h.rt->game().party;
    for (size_t i = size_t(p.party_size); i < kRosterCapacity; ++i) p.characters[i].party_status = 0xff;
    p.character_count = kRosterCapacity;
    h.render(true);
}

struct Card {
    Run &h;
    /** Alt+M, Save Game, and leave the menu open on the result. */
    std::string save() {
        h.key('m', true);
        h.down();
        h.key('\r');
        return footer(menu(h));
    }
    void close() { h.key('m', true); }
    /** Alt+M, Load Game (the menu stays open on the Load page). */
    void open_load() {
        h.key('m', true);
        h.down();
        h.down();
        h.key('\r');
    }
    void at(int x, int y, int hour, int minute) {
        DebugTeleportRequest r{};
        r.kind = DebugDestinationKind::Britannia;
        r.x = x;
        r.y = y;
        apply_debug_teleport(h.rt->command_context_for_test(), r);
        h.rt->game().time.hour = hour;
        h.rt->game().time.minute = minute;
        h.render(true);
    }
};

void test_system_menu() {
    std::printf("M  the System Menu\n");
    host_memory_save_forget_for_test();
    Run h(kParty);
    full_roster(h);
    Card c{h};
    h.key('m', true);
    const auto root = menu(h);
    dump("system-menu");
    check(lines(root) == "Resume | Save Game | Load Game | Settings | Return to Title",
          "M1 the rows name the actions: " + lines(root));
    std::string f[5];
    for (int i = 0; i < 5; ++i) {
        f[i] = footer(menu(h));
        h.down();
    }
    check(f[0] == "Alt+M or Mic: resume" && f[1] == "Saves now; the previous save becomes the backup" &&
              f[2] == "The latest save and its backup" && f[3] == "Alt+M or Mic: resume" &&
              f[4] == "Unsaved progress will be lost",
          "M2 the selected row says what it will do (Save: \"" + f[1] + "\"; Return to Title: \"" + f[4] + "\")");
    c.close();

    const std::string first = c.save();
    const bool transcript1 = h.transcript().find("Save complete") != std::string::npos;
    c.close();
    c.at(120, 110, 13, 5);
    const std::string second = c.save();
    dump("system-menu-saved");
    check(first == "Saved." && transcript1, "M3 the first save: the footer says \"" + first + "\" (the transcript line is kept)");
    check(second == "Saved. The previous save is now the backup.",
          "M4 a save over a save says what it overwrote: \"" + second + "\"");
    h.down();
    check(footer(menu(h)) == "The latest save and its backup", "M5 the notice lasts until the next key");
    c.close();
}

void test_load_page() {
    std::printf("L  Load Game\n");
    host_memory_save_forget_for_test();
    Run h(kParty);
    full_roster(h);
    Card c{h};
    const uint8_t home = uint8_t(h.rt->game().position.map.location);
    c.save();
    c.close();
    c.at(120, 110, 13, 5);
    c.save();
    c.close();
    // seq 1 went to slot 1, seq 2 to slot 0: the newer save is the LOWER slot here.
    c.open_load();
    auto v = menu(h);
    dump("load-game");
    const std::string latest = line(v, 0), backup = line(v, 1);
    const std::string home_name = home == 13 ? "Iolo's Hut" : "?";
    check(std::string(v.title) == "Load Game" && v.line_count == 2 && latest == "Latest: Avery, Britannia" &&
              backup == "Backup: Avery, " + home_name,
          "L1 two rows by age, leader and place: \"" + latest + "\" / \"" + backup + "\" (was \"Generation 1/2\" by slot)");
    const std::string d0 = footer(v);
    h.down();
    const std::string d1 = footer(menu(h));
    check(d0 == "4-5-139 13:05, party of 2. Enter loads" && d1 == "4-5-139 12:00, party of 2. Enter loads",
          "L2 the selected row's date, time and party: \"" + d0 + "\" / \"" + d1 + "\"");
    h.key('\r'); // Backup
    const bool backup_loaded = !h.rt->system_menu_open() && h.rt->game().position.map.location == home &&
                               h.rt->game().time.hour == 12 && h.transcript().find("Load complete") != std::string::npos;
    check(backup_loaded, "L3 Enter on Backup loads that older save (back at " + home_name + ", 12:00)");
    c.open_load();
    h.key('\r'); // Latest
    check(!h.rt->system_menu_open() && h.rt->game().position.map.location == 0 && h.rt->game().time.hour == 13,
          "L4 Enter on Latest is Continue: the newer save (Britannia, 13:05)");

    // A third save lands in the other physical slot; the rows still read by age.
    c.at(121, 110, 14, 10);
    c.save();
    c.close();
    c.open_load();
    v = menu(h);
    check(line(v, 0) == "Latest: Avery, Britannia" && line(v, 1) == "Backup: Avery, Britannia" &&
              footer(v) == "4-5-139 14:10, party of 2. Enter loads",
          "L5 after a third save (the newer save now in the HIGHER slot) Latest is still the newest: \"" + footer(v) + "\"");
    c.close();

    // The latest generation damaged behind the service's back.
    host_memory_save_damage_for_test();
    c.open_load();
    v = menu(h);
    const std::string dmg = footer(v);
    dump("load-game-damaged");
    check(line(v, 0) == "Latest: damaged" && line(v, 1) == "Backup: Avery, Britannia" &&
              dmg == "Damaged: Enter loads the backup instead",
          "L6 a damaged latest save says so, and what Enter does: \"" + dmg + "\"");
    h.key('\r');
    check(!h.rt->system_menu_open() && h.rt->game().time.hour == 13 && h.transcript().find("Load complete") != std::string::npos,
          "L7 ... and Enter on it loads the backup (Continue's fallback, unchanged)");
    // Saving now replaces the damaged generation, and the backup is the save
    // Continue just fell back to. A4-SAVE1: until then it replaced the OLDER,
    // only valid one, and this row read "Backup: damaged".
    c.at(122, 110, 15, 20);
    c.save();
    c.close();
    c.open_load();
    v = menu(h);
    h.down();
    const std::string kept = footer(menu(h));
    check(line(v, 0) == "Latest: Avery, Britannia" && line(v, 1) == "Backup: Avery, Britannia" &&
              kept == "4-5-139 13:05, party of 2. Enter loads",
          "L8a A4-SAVE1: a save after the fallback replaces the damaged latest; the backup is the 13:05 save: \"" + kept + "\"");
    c.close();
    // A damaged backup, then (the older generation damaged behind the service).
    host_memory_save_damage_older_for_test();
    c.open_load();
    v = menu(h);
    h.down();
    const std::string bd = footer(menu(h));
    h.key('\r');
    const std::string refused = footer(menu(h));
    check(line(v, 0) == "Latest: Avery, Britannia" && line(v, 1) == "Backup: damaged" &&
              bd == "Damaged: this backup cannot be loaded" && h.rt->system_menu_open() &&
              refused == "That backup is damaged and cannot load" && h.rt->game().time.hour == 15,
          "L8 a damaged backup: listed damaged, and Enter says it cannot load (was a silent no-op): \"" + refused + "\"");
    c.close();

    // A card with one save: no backup yet.
    host_memory_save_forget_for_test();
    Run one(kParty);
    full_roster(one);
    Card k{one};
    k.save();
    k.close();
    k.open_load();
    v = menu(one);
    one.down();
    const std::string none = footer(menu(one));
    one.key('\r');
    check(line(v, 1) == "Backup: none yet" && none == "Each save keeps the one before as backup" &&
              footer(menu(one)) == "No backup save yet" && one.rt->system_menu_open(),
          "L9 one save: \"Backup: none yet\", its footer explains backups, Enter says there is none");
    one.key('m', true);
    host_memory_save_forget_for_test();
    Run empty(kParty);
    full_roster(empty);
    Card e{empty};
    e.open_load();
    v = menu(empty);
    empty.key('\r');
    check(line(v, 0) == "Latest: no save yet" && footer(v) == "No save on this card yet" &&
              empty.transcript().find("No valid save") != std::string::npos,
          "L10 an empty card: \"Latest: no save yet\"; Enter still reports \"No valid save\" (unchanged)");
}

void test_title() {
    std::printf("F  the title\n");
    host_memory_save_forget_for_test();
    Run h(kParty);
    full_roster(h);
    Card c{h};
    const uint8_t home = uint8_t(h.rt->game().position.map.location);
    c.save();
    c.close();
    c.at(120, 110, 13, 5);
    c.save();
    c.close();
    h.return_to_title();
    h.key(' '); // Title -> main menu
    const std::string menu0 = footer(h.rt->frontend_view());
    h.down();
    const auto create = h.rt->frontend_view();
    dump("main-menu-create");
    check(menu0 == "Select: arrows / Enter / J C T U A R" &&
              footer(create) == "New game: your current save becomes the backup",
          "F1 \"Create New Character\" warns that the current save becomes the backup: \"" + footer(create) + "\"");
    h.up();
    h.key('\r'); // Journey Onward
    auto v = h.rt->frontend_view();
    dump("journey-onward");
    check(h.rt->frontend_state() == FrontendState::Continue && lines(v) == "Continue | Load Game" &&
              std::string(v.subtitle) == "Latest save: Avery, Britannia" && footer(v) == "Enter continues; Mic returns",
          "F2 Journey Onward: \"" + lines(v) + "\", naming the save Continue loads: \"" + v.subtitle + "\"");
    // The shell's subtitle (y=15..22) keeps its last glyph rows: the body clear
    // used to start at y=20 and cut them (no shell page had a subtitle before).
    const int tail = lit(8, 20, 304, 2);
    check(tail > 0, "F2b the subtitle is drawn whole: its bottom glyph rows (y=20..21) survive the body (" + n(tail) +
                        " px lit)");
    h.down();
    h.key('\r'); // Load Game
    v = h.rt->frontend_view();
    dump("title-load-game");
    check(h.rt->frontend_state() == FrontendState::Load && std::string(v.title) == "Load Game" &&
              line(v, 0) == "Latest: Avery, Britannia" && line(v, 1) == std::string("Backup: Avery, ") +
              (home == 13 ? "Iolo's Hut" : "?") && footer(v) == "4-5-139 13:05, party of 2. Enter loads",
          "F3 the title's Load Game lists the same rows: \"" + lines(v) + "\"");
    h.mic();
    v = h.rt->frontend_view();
    check(h.rt->frontend_state() == FrontendState::Continue && v.selected_line == 1,
          "F4 Back from Load Game returns to Journey Onward, on Load Game (it went to the main menu)");
    h.key('\r');
    h.down();
    h.key('\r'); // Backup
    check(!h.rt->frontend_open() && h.rt->game().position.map.location == home && h.rt->game().time.hour == 12,
          "F5 Backup loads the older save and enters the game");
}

void test_fit() {
    std::printf("Z  fit\n");
    host_memory_save_forget_for_test();
    Run h({{"Shamino12", 'G', 100}, {"Iolo", 'G', 100}});
    full_roster(h);
    Card c{h};
    DebugTeleportRequest r{};
    r.kind = DebugDestinationKind::SmallMap;
    r.location = 17; // Lord British's Castle: the longest caption a row meets
    r.standard_entry = true;
    const bool moved = apply_debug_teleport(h.rt->command_context_for_test(), r).status == DebugTeleportStatus::Applied;
    h.rt->game().time = {139, 12, 28, 23, 59};
    h.render(true);
    c.save();
    c.close();
    c.save();
    c.close();
    c.open_load();
    size_t widest = 0, widest_footer = 0;
    std::string row0;
    for (int i = 0; i < 2; ++i) {
        const auto v = menu(h);
        if (!i) row0 = line(v, 0);
        for (size_t k = 0; k < v.line_count; ++k) widest = std::max(widest, line(v, k).size());
        widest_footer = std::max(widest_footer, footer(v).size());
        h.down();
    }
    dump("load-game-long");
    check(moved && row0 == "Latest: Shamino12, Lord British's Ca" && widest <= 36 && widest_footer <= 50,
          "Z1 the longest row is cut to the 36 cells Large text shows (\"" + row0 + "\"); footers fit 50 (" +
              n(long(widest_footer)) + ")");
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: a4_ui2_save_menu_runtime <pack> [--dump <dir>]\n");
        return 2;
    }
    g_dump = arg_after(argc, argv, "--dump");
    if (!load_pack(argv[1])) {
        std::printf("RED the pack does not load -- the menu checks cannot run\n");
        return 1;
    }
    test_system_menu();
    test_load_page();
    test_title();
    test_fit();
    std::printf("A4-UI2 save menu runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
