// Alpha 4 UI Batch 2 (targets/tdeck/ALPHA4_UI.md section 2.3) -- the save and
// load menus, through the REAL AlphaRuntime on the REAL tdeck_board.cpp over
// the memory card (the production generation gate).
//
// A4-UI2 listed one journey's two generations as "Latest" and "Backup".
// Since Alpha 4 A4-SAVE2 (section 4) the card holds three manual slots, each
// a journey with its own two generations; the menus list the slots and the
// generations are the hidden recovery inside each. This test keeps UI2's
// purposes on the new presentation (every expectation it changed is listed in
// ALPHA4_UI.md section 4.7):
//
//   M  System Menu: Save Game / Load Game; the selected row says what it does;
//      Save Game lists Slots 1-3, an empty slot saves at once, an occupied one
//      asks "Overwrite Slot N?" with No first, and No / Back save nothing;
//      after a save the footer says where it went
//   L  Load Game: Slots 1-3 with the leader and place, and (for the selected
//      row) the date, time and party size; each slot restores its own journey;
//      an empty or damaged slot says so instead of loading; a slot whose last
//      save is damaged says so and loads the save before it (the A4-SAVE1
//      recovery, hidden otherwise); saving into it keeps that one
//   F  the title: "Create New Character" names the empty slot it will use, and
//      with none empty asks which to replace (No first); Journey Onward names
//      the save Continue loads and its slot; its Load Game page lists the same
//      rows and returns to Journey Onward
//   Z  every row fits the Large text size (36 cells), every footer 50
//
// Alpha 4 A4-UI3 (ALPHA4_UI.md section 6) changed these expectations, on
// purpose: a slot is two rows ("Slot N  <leader>" + tags, then its place);
// the journey's slot is CURRENT in game and Continue's LATEST on the title;
// a recovered slot is tagged RECOVERED and its load says so; a save from the
// menu returns to the game with "Save complete: Slot N" (M4, M9, M10); the
// confirm pages name the slot's save ("Avery, <place>"); Journey Onward reads
// "Latest: Slot N, <leader>, <place>"; a name keeps 8 letters (Z1).
//
//   a4_ui2_save_menu_runtime <openu5-alpha1-resources.bin> [--dump <dir>]
#include "a4_ui2_harness.h"
#include "openu5/debug_developer.h"

namespace tdeck {
void host_memory_save_forget_for_test();
void host_memory_save_damage_for_test();
void host_memory_save_damage_older_for_test();
int host_memory_save_generations_for_test();
} // namespace tdeck

using namespace a4_ui2;
using tdeck::host_memory_save_damage_for_test;
using tdeck::host_memory_save_damage_older_for_test;
using tdeck::host_memory_save_forget_for_test;
using tdeck::host_memory_save_generations_for_test;

namespace {
const std::vector<Member> kParty = {{"Avery", 'G', 100}, {"Iolo", 'G', 100}};

std::string line(const FrontendView &v, size_t i) { return i < v.line_count && v.lines[i] ? v.lines[i] : ""; }
std::string footer(const FrontendView &v) { return v.footer ? v.footer : ""; }
std::string title(const FrontendView &v) { return v.title ? v.title : ""; }
// Alpha 4 A4-UI3: a slot page's line i has a detail row (the place); shown
// here after " / ", its indent dropped.
std::string detail(const FrontendView &v, size_t i) {
    const char *d = i < kSaveSlotCount ? v.details[i] : nullptr;
    if (!d) return "";
    while (*d == ' ') ++d;
    return d;
}
std::string lines(const FrontendView &v) {
    std::string out;
    for (size_t i = 0; i < v.line_count; ++i) {
        out += (i ? " | " : "") + line(v, i);
        if (!detail(v, i).empty()) out += " / " + detail(v, i);
    }
    return out;
}
std::string last_line(Run &h) {
    std::string t = h.transcript();
    if (!t.empty()) t.pop_back();
    const size_t at = t.rfind('\n');
    return at == std::string::npos ? t : t.substr(at + 1);
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
    /** Move a slot page's cursor to `slot` (0-2). */
    void select(int slot, bool frontend = false) {
        for (int i = 0; i < 3; ++i) {
            const auto v = frontend ? h.rt->frontend_view() : menu(h);
            if (v.selected_line == slot) return;
            h.down();
        }
    }
    /** Alt+M, Save Game, the slot; an occupied slot's question answered Yes
     *  (or No). A4-UI3: a save returns to the game (its transcript line is
     *  the result); otherwise the menu stays open on its footer. */
    std::string save(int slot, bool yes = true) {
        h.key('m', true);
        h.down();
        h.key('\r');
        select(slot);
        h.key('\r');
        if (title(menu(h)).rfind("Overwrite", 0) == 0) {
            if (yes) h.down();
            h.key('\r');
        }
        return h.rt->system_menu_open() ? footer(menu(h)) : last_line(h);
    }
    void close() {
        if (h.rt->system_menu_open()) h.key('m', true);
    }
    /** Alt+M, Load Game (the menu stays open on the Load page). */
    void open_load() {
        h.key('m', true);
        h.down();
        h.down();
        h.key('\r');
    }
    void load(int slot) {
        open_load();
        select(slot);
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
    int hour() const { return h.rt->game().time.hour; }
    int minute() const { return h.rt->game().time.minute; }
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
    check(f[0] == "Alt+M or Mic: resume" && f[1] == "Choose a slot to save this journey in" &&
              f[2] == "Choose a saved journey to load" && f[3] == "Alt+M or Mic: resume" &&
              f[4] == "Unsaved progress will be lost",
          "M2 the selected row says what it will do (Save Game: \"" + f[1] + "\"; Return to Title: \"" + f[4] + "\")");
    c.close();

    // The Save page on a blank card: three empty slots, the cursor on Slot 1.
    h.key('m', true);
    h.down();
    h.key('\r');
    auto v = menu(h);
    dump("save-game");
    check(title(v) == "Save Game" && lines(v) == "Slot 1  EMPTY | Slot 2  EMPTY | Slot 3  EMPTY" && v.selected_line == 0 &&
              footer(v) == "Empty. Enter saves here",
          "M3 Save Game lists Slots 1-3 (blank card: all empty, on Slot 1): " + lines(v));
    h.key('\r');
    const std::string first = last_line(h);
    check(first == "Save complete: Slot 1" && !h.rt->system_menu_open() && host_memory_save_generations_for_test() == 1,
          "M4 an empty slot saves at once and returns to the game: \"" + first + "\" (A4-UI3)");
    c.close();

    // An occupied slot asks first, and No is the answer Enter gives by default.
    c.at(120, 110, 13, 5);
    h.key('m', true);
    h.down();
    h.key('\r');
    v = menu(h);
    const bool on_journey = v.selected_line == 0 && footer(v) == "4-5-139 12:00, party of 2. Enter replaces it";
    h.key('\r');
    v = menu(h);
    dump("overwrite-confirm");
    check(on_journey && title(v) == "Overwrite Slot 1?" && lines(v) == "No, keep it | Yes, overwrite" &&
              v.selected_line == 0 && std::string(v.subtitle).rfind("Avery, ", 0) == 0 &&
              footer(v) == "Keeps the saved journey",
          "M5 an occupied slot (the journey's own, where the page starts) asks \"" + title(v) + "\", No selected");
    const std::string before_no = h.transcript();
    h.key('\r'); // No
    v = menu(h);
    check(title(v) == "Save Game" && v.selected_line == 0 && footer(v) == "Slot 1 kept" && h.transcript() == before_no &&
              host_memory_save_generations_for_test() == 1,
          "M6 No: back on the Save page, \"" + footer(v) + "\", nothing written (still one generation)");
    h.key('\r');
    h.key('\b'); // Back from the question
    v = menu(h);
    check(title(v) == "Save Game" && h.transcript() == before_no && host_memory_save_generations_for_test() == 1,
          "M7 Back from the question returns to the Save page and writes nothing");
    h.key('\r');
    h.down();
    check(footer(menu(h)) == "The saved journey in this slot is replaced", "M8 Yes says what it does");
    h.key('\r');
    const std::string second = last_line(h);
    check(second == "Save complete: Slot 1" && !h.rt->system_menu_open() && host_memory_save_generations_for_test() == 2,
          "M9 Yes saves and returns to the game: \"" + second + "\" (the slot now keeps two generations, the older one hidden)");
    h.key('m', true);
    check(menu(h).selected_line == 0 && footer(menu(h)) == "Alt+M or Mic: resume",
          "M10 the next open starts on Resume with no stale notice (A4-UI3)");
    h.down();
    h.down();
    h.key('\r'); // Load Game
    check(footer(menu(h)) == "4-5-139 13:05, party of 2. Enter loads",
          "M11 the Load page lists the save just made (13:05): \"" + footer(menu(h)) + "\"");
    c.close();
}

void test_load_page() {
    std::printf("L  Load Game\n");
    host_memory_save_forget_for_test();
    Run h(kParty);
    full_roster(h);
    Card c{h};
    const uint8_t home = uint8_t(h.rt->game().position.map.location);
    const std::string home_name = home == 13 ? "Iolo's Hut" : "?";
    c.save(0);          // Slot 1: home, 12:00
    c.close();
    c.at(120, 110, 13, 5);
    c.save(1);          // Slot 2: Britannia, 13:05
    c.close();
    c.open_load();
    auto v = menu(h);
    dump("load-game");
    check(title(v) == "Load Game" && lines(v) == "Slot 1  Avery / " + home_name + " | Slot 2  Avery     CURRENT / Britannia | Slot 3  EMPTY" &&
              v.selected_line == 1,
          "L1 three slot rows with leader and place, on the journey's slot (Slot 2): " + lines(v));
    const std::string d2 = footer(v);
    h.up();
    const std::string d1 = footer(menu(h));
    check(d2 == "4-5-139 13:05, party of 2. Enter loads" && d1 == "4-5-139 12:00, party of 2. Enter loads",
          "L2 the selected slot's date, time and party: \"" + d2 + "\" / \"" + d1 + "\"");
    h.key('\r'); // Slot 1
    check(!h.rt->system_menu_open() && h.rt->game().position.map.location == home && c.hour() == 12 &&
              h.transcript().find("Load complete") != std::string::npos,
          "L3 Enter on Slot 1 restores Slot 1's journey (back at " + home_name + ", 12:00)");
    c.open_load();
    const int start = menu(h).selected_line;
    c.select(1);
    h.key('\r');
    check(start == 0 && !h.rt->system_menu_open() && h.rt->game().position.map.location == 0 && c.hour() == 13,
          "L4 the page now starts on Slot 1 (the loaded journey); Slot 2 restores its own (Britannia, 13:05)");
    c.open_load();
    c.select(2);
    const std::string empty_detail = footer(menu(h));
    h.key('\r');
    check(empty_detail == "Empty slot" && footer(menu(h)) == "Slot 3 is empty" && h.rt->system_menu_open() && c.hour() == 13,
          "L5 an empty slot says so and cannot be loaded (the menu stays, nothing changes)");
    c.close();

    // Slot 2 saved again: two generations. Its latest damaged behind the service.
    c.at(121, 110, 14, 10);
    c.save(1);
    c.close();
    host_memory_save_damage_for_test();
    c.open_load();
    c.select(1);
    v = menu(h);
    dump("load-game-recovered");
    const std::string rec = footer(v);
    check(line(v, 1) == "Slot 2  Avery     CURRENT, RECOVERED" && detail(v, 1) == "Britannia" &&
              rec == "Last save damaged; Enter loads the one before",
          "L6 a slot whose last save is damaged says so, and what Enter does: \"" + rec + "\"");
    h.key('\r');
    check(!h.rt->system_menu_open() && c.hour() == 13 && c.minute() == 5 && h.transcript().find("Load complete") != std::string::npos &&
              last_line(h) == "Recovered previous save (Slot 2)",
          "L7 ... and Enter restores the save before it (13:05), the A4-SAVE1 fallback inside the slot, and says so");
    // A save now replaces the damaged generation and keeps the 13:05 one.
    c.at(122, 110, 15, 20);
    c.save(1);
    c.close();
    c.open_load();
    c.select(1);
    const std::string replaced = footer(menu(h));
    c.close();
    host_memory_save_damage_for_test();   // the 15:20 save torn in its turn
    c.load(1);
    check(replaced == "4-5-139 15:20, party of 2. Enter loads" && c.hour() == 13 && c.minute() == 5,
          "L8 A4-SAVE1 in a slot: the save after the fallback is the slot's latest (\"" + replaced +
              "\") and the 13:05 save it fell back to is still its hidden recovery");
    // Both generations damaged.
    host_memory_save_damage_older_for_test();
    c.open_load();
    c.select(1);
    v = menu(h);
    const std::string dd = footer(v);
    h.key('\r');
    const std::string refused = footer(menu(h));
    check(line(v, 1) == "Slot 2  DAMAGED   CURRENT" && detail(v, 1) == "Cannot be loaded" &&
              dd == "Damaged: this slot cannot be loaded" && h.rt->system_menu_open() &&
              refused == "Slot 2 is damaged and cannot load" && c.hour() == 13,
          "L9 nothing loadable: listed damaged, and Enter says it cannot load: \"" + refused + "\"");
    c.select(0);
    h.key('\r');
    check(!h.rt->system_menu_open() && h.rt->game().position.map.location == home && c.hour() == 12,
          "L10 Slot 1 is untouched by everything done to Slot 2 (" + home_name + ", 12:00)");

    host_memory_save_forget_for_test();
    Run empty(kParty);
    full_roster(empty);
    Card e{empty};
    e.open_load();
    v = menu(empty);
    empty.key('\r');
    check(lines(v) == "Slot 1  EMPTY | Slot 2  EMPTY | Slot 3  EMPTY" && footer(v) == "Empty slot" &&
              footer(menu(empty)) == "Slot 1 is empty" && empty.transcript().find("Load complete") == std::string::npos,
          "L11 an empty card: three empty slots, none loads");
}

void test_title() {
    std::printf("F  the title\n");
    host_memory_save_forget_for_test();
    Run h(kParty);
    full_roster(h);
    Card c{h};
    const uint8_t home = uint8_t(h.rt->game().position.map.location);
    const std::string home_name = home == 13 ? "Iolo's Hut" : "?";
    c.save(0);          // Slot 1: home, 12:00
    c.close();
    c.at(120, 110, 13, 5);
    c.save(2);          // Slot 3: Britannia, 13:05 (the newest)
    c.close();
    h.return_to_title();
    h.key(' '); // Title -> main menu
    const std::string menu0 = footer(h.rt->frontend_view());
    h.down();
    const auto create = h.rt->frontend_view();
    dump("main-menu-create");
    check(menu0 == "Select: arrows / Enter / J C T U A R" && footer(create) == "New journey: saved in empty Slot 2",
          "F1 \"Create New Character\" names the empty slot it will use: \"" + footer(create) + "\"");
    h.up();
    h.key('\r'); // Journey Onward
    auto v = h.rt->frontend_view();
    dump("journey-onward");
    check(h.rt->frontend_state() == FrontendState::Continue && lines(v) == "Continue | Load Game" &&
              std::string(v.subtitle) == "Latest: Slot 3, Avery, Britannia" && footer(v) == "Enter continues Slot 3; Mic returns",
          "F2 Journey Onward: \"" + lines(v) + "\", naming the save Continue loads: \"" + v.subtitle + "\" / \"" + footer(v) + "\"");
    // The shell's subtitle (y=15..22) keeps its last glyph rows: the body clear
    // used to start at y=20 and cut them (no shell page had a subtitle before).
    const int tail = lit(8, 20, 304, 2);
    check(tail > 0, "F2b the subtitle is drawn whole: its bottom glyph rows (y=20..21) survive the body (" + n(tail) +
                        " px lit)");
    h.down();
    h.key('\r'); // Load Game
    v = h.rt->frontend_view();
    dump("title-load-game");
    check(h.rt->frontend_state() == FrontendState::Load && title(v) == "Load Game" &&
              lines(v) == "Slot 1  Avery / " + home_name + " | Slot 2  EMPTY | Slot 3  Avery     LATEST / Britannia" && v.selected_line == 2 &&
              footer(v) == "4-5-139 13:05, party of 2. Enter loads",
          "F3 the title's Load Game lists the same slots, on Continue's: \"" + lines(v) + "\"");
    c.select(1, true);
    h.key('\r'); // Slot 2: empty
    v = h.rt->frontend_view();
    check(h.rt->frontend_state() == FrontendState::Load && footer(v) == "Slot 2 is empty" && h.rt->frontend_open(),
          "F3b the title's empty slot says so and does not load (still on Load Game)");
    h.mic();
    v = h.rt->frontend_view();
    check(h.rt->frontend_state() == FrontendState::Continue && v.selected_line == 1,
          "F4 Back from Load Game returns to Journey Onward, on Load Game");
    h.key('\r');
    c.select(0, true);
    h.key('\r'); // Slot 1
    check(!h.rt->frontend_open() && h.rt->game().position.map.location == home && c.hour() == 12,
          "F5 Slot 1 loads its own journey and enters the game");
    // Every slot in use: Create New Character asks which to replace.
    c.save(1);
    c.close();
    h.return_to_title();
    h.key(' ');
    h.down();
    const std::string full = footer(h.rt->frontend_view());
    const int generations = host_memory_save_generations_for_test();
    h.key('\r');
    v = h.rt->frontend_view();
    dump("new-journey-slot");
    check(full == "Slots full: you choose one to replace" && h.rt->frontend_state() == FrontendState::NewJourneySlot &&
              title(v) == "New Journey" && v.line_count == 3,
          "F6 with every slot in use Create New Character lists them to choose one: \"" + full + "\"");
    h.key('\r');
    v = h.rt->frontend_view();
    const bool asked = title(v).rfind("Replace Slot ", 0) == 0 && v.selected_line == 0 && line(v, 0) == "No, keep it";
    h.key('\r'); // No
    v = h.rt->frontend_view();
    const bool back_to_list = h.rt->frontend_state() == FrontendState::NewJourneySlot && title(v) == "New Journey";
    h.mic();
    check(asked && back_to_list && h.rt->frontend_state() == FrontendState::MainMenu &&
              host_memory_save_generations_for_test() == generations,
          "F7 \"Replace Slot N?\" starts on No; No and Back leave every slot as it was");
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
    c.save(0);
    c.close();
    c.save(0);
    c.close();
    size_t widest = 0, widest_footer = 0;
    std::string row0;
    auto measure = [&](bool saving) {
        h.key('m', true);
        h.down();
        if (!saving) h.down();
        h.key('\r');
        for (int i = 0; i < 3; ++i) {
            const auto v = menu(h);
            if (!i && !saving) row0 = line(v, 0) + "|" + (v.details[0] ? v.details[0] : "");
            for (size_t k = 0; k < v.line_count; ++k)
                widest = std::max({widest, line(v, k).size(), k < kSaveSlotCount && v.details[k] ? std::strlen(v.details[k]) : size_t(0)});
            widest_footer = std::max(widest_footer, footer(v).size());
            h.down();
        }
        c.close();
    };
    measure(false);
    measure(true);
    dump("load-game-long");
    // A4-UI3: the place has its own row, so neither is cut; a name keeps 8 letters (the game's limit).
    check(moved && row0 == "Slot 1  Shamino1  CURRENT|        Lord British's Castle" && widest <= 36 && widest_footer <= 50,
          "Z1 the longest rows fit the 36 cells Large text shows (\"" + row0 + "\"); footers fit 50 (" +
              n(long(widest_footer)) + ")");
    // Every footer the slot pages can show.
    FrontendSaveCatalog k{};
    k.slots[0].status = SaveSlotStatus::Saved;
    k.slots[0].shown.month = 12; k.slots[0].shown.day = 28; k.slots[0].shown.year = 139;
    k.slots[0].shown.hour = 23; k.slots[0].shown.minute = 59; k.slots[0].shown.party = 6;
    k.slots[1].status = SaveSlotStatus::Recovered;
    k.slots[2].status = SaveSlotStatus::Damaged;
    size_t longest = 0;
    for (int s = 0; s < 3; ++s)
        for (bool saving : {false, true}) {
            char d[96];
            format_slot_detail(d, sizeof(d), k, s, saving);
            longest = std::max(longest, std::strlen(d));
        }
    k.slots[0].status = SaveSlotStatus::Empty;
    for (bool saving : {false, true}) {
        char d[96];
        format_slot_detail(d, sizeof(d), k, 0, saving);
        longest = std::max(longest, std::strlen(d));
    }
    check(longest <= 50, "Z2 every slot footer (saved, recovered, damaged, empty; load and save) fits 50 (" + n(long(longest)) + ")");
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
