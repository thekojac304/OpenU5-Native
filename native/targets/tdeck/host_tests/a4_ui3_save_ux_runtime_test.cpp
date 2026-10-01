// Alpha 4 A4-UI3 (targets/tdeck/ALPHA4_UI.md section 6) -- the save/load UX on
// the device: the REAL AlphaRuntime on the REAL tdeck_board.cpp over the fake
// ST7789 (the UI2 harness), with the REAL alpha_save.cpp over the fake SD card
// (host_tests/sd_shims, as a4_save2_slots_runtime), so every slot row, tag and
// footer here comes from the production save list of real files.
//
//   P  the panel: a slot is two rows (its line, then its place in grey); the
//      selection covers both rows and moves with them; Large text fits
//   S  Save Game: an empty slot saves at once and returns to the game; an
//      occupied one asks "Overwrite Slot N?" naming its own save, No first;
//      No, Back and the Mic write nothing; Yes saves; a failed save keeps the
//      menu and says truthfully what the slot still holds
//   L  Load Game: CURRENT follows the journey; an empty slot refuses without
//      touching the card; a recovered slot lists (and loads) the save before
//      the newest and says so; a damaged slot is neither EMPTY nor a journey;
//      a slot that fails as it loads loads nothing else in its place
//   C  the title: Journey Onward names Continue's slot and journey, its Load
//      Game marks it LATEST; Continue is still one key; New Journey's slot
//      becomes CURRENT
//   X  PC Save Transfer: reachable from the title, its pickers use the slot
//      rows, the occupied-slot question starts on No and the Mic cancels it
//   W  warm opens read only the commit records (no new storage work)
//
//   a4_ui3_save_ux_runtime <openu5-alpha1-resources.bin> [original/u5/ultima5] [--dump <dir>]
#include "a4_ui2_harness.h"
#include "../main/alpha_save.h"
#include "../main/device_ui_views.h"
#include "openu5/debug_developer.h"
#include "sd_shims/host_sd_card.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>

using namespace a4_ui2;
namespace fs = std::filesystem;

namespace {
const std::vector<Member> kParty = {{"Kojac", 'G', 100}, {"Iolo", 'G', 100}};
constexpr const char *kSaves = "/sd/ultima5/saves";
constexpr const char *kImport = "/sd/ultima5/import";

std::string str(const char *s) { return s ? s : ""; }
std::string q(const std::string &s) { return "\"" + s + "\""; }
std::string page(const FrontendView &v) {
    std::string out;
    for (size_t i = 0; i < v.line_count; ++i) {
        out += (i ? " ; " : "") + str(v.lines[i]);
        const char *d = i < size_t(kSaveSlotCount) ? v.details[i] : nullptr;
        while (d && *d == ' ') ++d;
        if (d && *d) out += " / " + std::string(d);
    }
    return out;
}

// ---- the card ----------------------------------------------------------------
using Card = std::map<std::string, std::string>;
Card g_healthy;   // the Load group's card, every generation accepted
Card take_card() {
    Card c;
    std::error_code ec;
    const auto dir = fs::path(host_sd::root()) / "sd/ultima5/saves";
    if (!fs::exists(dir, ec)) return c;
    for (const auto &e : fs::directory_iterator(dir, ec)) {
        if (!e.is_regular_file()) continue;
        const std::string name = e.path().filename().string();
        std::string bytes;
        host_sd::read_card_file((std::string(kSaves) + "/" + name).c_str(), bytes);
        c[name] = bytes;
    }
    return c;
}
void put_card(const Card &c) {
    host_sd::format_card();
    std::error_code ec;
    fs::create_directories(fs::path(host_sd::root()) / "sd/ultima5/saves", ec);
    for (const auto &[name, bytes] : c) host_sd::write_card_file((std::string(kSaves) + "/" + name).c_str(), bytes);
}
std::string gen_path(int slot, int gen, const char *ext) {
    char p[96];
    if (slot == 0) std::snprintf(p, sizeof(p), "%s/alpha1-g%d.%s", kSaves, gen, ext);
    else std::snprintf(p, sizeof(p), "%s/alpha1-s%d-g%d.%s", kSaves, slot + 1, gen, ext);
    return p;
}
Card slot_of(const Card &c, int slot) {
    const std::string prefix = slot == 0 ? "alpha1-g" : slot == 1 ? "alpha1-s2-g" : "alpha1-s3-g";
    Card out;
    for (const auto &[name, bytes] : c)
        if (name.rfind(prefix, 0) == 0) out[name] = bytes;
    return out;
}
/** The generation holding the slot's newest commit (a cold look). */
int newest_gen(int slot) {
    FrontendSaveSlot g[2];
    tdeck::AlphaSaveService().inspect_generations(slot, g);
    return g[1].sequence > g[0].sequence ? 1 : 0;
}
/** Damage one generation: its commit record's game-file CRC (bytes 16-19 of
 *  AlphaSaveCommit) no longer matches the file. A changed record, so the
 *  runtime's save list re-reads it (a torn file under an unchanged record is
 *  the save list's to trust until a load; a4_save3 notes the same). */
void tear(int slot, int gen) {
    std::string bytes;
    host_sd::read_card_file(gen_path(slot, gen, "commit").c_str(), bytes);
    if (bytes.size() > 16) bytes[16] = char(bytes[16] ^ 0x5a);
    host_sd::write_card_file(gen_path(slot, gen, "commit").c_str(), bytes);
}

// ---- the runtime -------------------------------------------------------------
struct Play {
    Run &h;
    FrontendView menu() const { return h.rt->system_menu_view(); }
    FrontendView front() const { return h.rt->frontend_view(); }
    std::string title() const { return str(menu().title); }
    std::string footer() const { return str(menu().footer); }
    std::string last_line() const {
        std::string t = h.transcript();
        if (!t.empty()) t.pop_back();
        const size_t at = t.rfind('\n');
        return at == std::string::npos ? t : t.substr(at + 1);
    }
    size_t mark_at = 0;
    void mark() { mark_at = h.rt->ui()->transcript_size(); }
    /** A transcript line since mark() containing `needle`. */
    bool saw(const std::string &needle) const {
        for (size_t i = mark_at; i < h.rt->ui()->transcript_size(); ++i)
            if (const auto *b = h.rt->ui()->transcript_at(i); b && std::strstr(b->text, needle.c_str())) return true;
        return false;
    }
    void select(int slot, bool frontend = false) {
        for (int i = 0; i < 3; ++i) {
            if ((frontend ? front() : menu()).selected_line == slot) return;
            h.down();
        }
    }
    void open_save() { h.key('m', true); h.down(); h.key('\r'); }
    void open_load() { h.key('m', true); h.down(); h.down(); h.key('\r'); }
    void close() { if (h.rt->system_menu_open()) h.key('m', true); }
    /** Save into `slot` through the menu, answering an Overwrite question Yes. */
    void save(int slot) {
        open_save();
        select(slot);
        h.key('\r');
        if (title().rfind("Overwrite", 0) == 0) { h.down(); h.key('\r'); }
    }
    /** A key with no frame drawn after it. The host fixture has no creation
     *  canvas (initialize() allocates it on the device), so the creation quiz
     *  cannot be drawn here: its keys go in unrendered. */
    void key_quiet(uint8_t code) {
        openu5_host_virtual_clock_us() += 100000;
        tdeck::RawInputEvent e{};
        e.kind = RawInputKind::Keyboard;
        e.transition = tdeck::KeyTransition::Pressed;
        e.code = e.base_code = code;
        h.raw(e);
    }
    void leader(const char *name, uint16_t gold) {
        auto &g = h.rt->game();
        std::snprintf(g.party.characters[0].name, sizeof(g.party.characters[0].name), "%s", name);
        g.gold = gold;
        h.render(true);
    }
    void britannia(int x, int y) {
        DebugTeleportRequest r{};
        r.kind = DebugDestinationKind::Britannia;
        r.x = x;
        r.y = y;
        apply_debug_teleport(h.rt->command_context_for_test(), r);
        h.render(true);
    }
    uint16_t gold() const { return h.rt->game().gold; }
};
/** A real game's roster: all 16 records, those past the party marked out of it. */
void full_roster(Run &h) {
    auto &p = h.rt->game().party;
    for (size_t i = size_t(p.party_size); i < kRosterCapacity; ++i) p.characters[i].party_status = 0xff;
    p.character_count = kRosterCapacity;
    h.render(true);
}

// ---- the panel ---------------------------------------------------------------
int count_colour(int x0, int y0, int w, int hgt, uint16_t c) {
    int k = 0;
    for (int y = y0; y < y0 + hgt; ++y)
        for (int x = x0; x < x0 + w; ++x) k += px(x, y) == c;
    return k;
}
struct Rows {
    int y0 = 23, step = 11, box = 10;
    explicit Rows(uint8_t size = 1) {
        const auto m = tdeck::ui_text_metrics(size);
        step = m.line_height + 3;
        box = m.line_height + 2;
    }
    int y(int row) const { return y0 + row * step; }
    bool inverted(int row) const { return count_colour(8, y(row), 304, box, kWhite) * 2 > 304 * box; }
    bool plain(int row) const { return count_colour(8, y(row), 304, box, kBlack) * 2 > 304 * box; }
    /** Ink in the row's text, and of it how much is the footers' grey. */
    int ink(int row) const { return lit(8, y(row), 304, box); }
    int grey(int row) const { return count_colour(8, y(row), 304, box, kDim); }
    int white(int row) const { return count_colour(8, y(row), 304, box, kWhite); }
};
std::string row_states(const Rows &r, int n) {
    std::string s;
    for (int i = 0; i < n; ++i) s += r.inverted(i) ? 'I' : r.plain(i) ? '.' : '?';
    return s;
}

// ---- P + S: the Save page on the panel, and saving -------------------------------
void test_save(Run &h, Play &p) {
    std::printf("\nP/S  Save Game on the panel and on the card\n");
    p.leader("Kojac", 101);
    p.open_save();
    auto v = p.menu();
    dump("ui3-save-empty");
    check(p.title() == "Save Game" && page(v) == "Slot 1  EMPTY ; Slot 2  EMPTY ; Slot 3  EMPTY" && v.selected_line == 0 &&
              p.footer() == "Empty. Enter saves here",
          "S1 a blank card: three EMPTY slots, nothing CURRENT, on Slot 1: " + page(v));
    h.key('\r');
    const Card one = take_card();
    check(!h.rt->system_menu_open() && p.last_line() == "Save complete: Slot 1" && slot_of(one, 0).size() == 4 &&
              slot_of(one, 1).empty() && slot_of(one, 2).empty(),
          "S2 an empty slot saves at once and returns to the game: " + q(p.last_line()) + " (Slot 1's four files, nothing else)");
    p.britannia(120, 110);
    p.leader("Avery", 202);
    p.save(1);
    check(!h.rt->system_menu_open() && p.last_line() == "Save complete: Slot 2", "S3 a second slot saves the same way");
    p.open_save();
    v = p.menu();
    const Rows r;
    dump("ui3-save-mixed");
    const std::string want = "Slot 1  Kojac / " + std::string("Iolo's Hut") + " ; Slot 2  Avery     CURRENT / Britannia ; Slot 3  EMPTY";
    check(page(v) == want && v.selected_line == 1,
          "S4 the Save page: leader and place per slot, the journey's slot CURRENT and selected: " + page(v));
    const std::string first = row_states(r, 7);
    check(first == "..II...", "P1 the selected slot's two rows are both in reverse video, the others plain (" + first + ")");
    check(r.grey(1) > 0 && r.white(1) == 0 && r.white(0) > 0,
          "P2 an unselected slot's place row is grey (" + n(r.grey(1)) + " px) under its white line");
    h.down();
    const std::string moved = row_states(r, 7);
    check(moved == "....II." && r.grey(3) > 0 && r.white(3) == 0,
          "P3 the selection moves both rows and leaves the old ones as they were (" + moved + ")");
    h.down();   // from Slot 3: wraps to Slot 1
    check(p.menu().selected_line == 0 && row_states(r, 7) == "II.....", "P4 Down from Slot 3 wraps to Slot 1 (" + row_states(r, 7) + ")");
    // Large text: the same page fits the 304 px rows and stops above the footer.
    h.board.show_frontend(p.menu(), nullptr, nullptr, nullptr, nullptr, 2);
    dump("ui3-save-large");
    const Rows large(2);
    int l = 0, t = 0, rr = 0, b = 0;
    ink_box(3, 23, 314, 200, l, t, rr, b);
    check(row_states(large, 7) == "II....." && rr <= 311 && b < large.y(6) && lit(8, 228, 304, 9) > 0,
          "P5 Large text: two rows a slot, the selection on both, ink within x<=311 and above y=" + n(large.y(6)) +
              " (right " + n(rr) + ", bottom " + n(b) + ")");
    h.render(true);

    // Overwrite: the question names the slot and its own save.
    h.key('\r');   // Slot 1 (Kojac)
    v = p.menu();
    dump("ui3-overwrite");
    check(p.title() == "Overwrite Slot 1?" && str(v.subtitle) == "Kojac, Iolo's Hut" && v.selected_line == 0 &&
              page(v) == "No, keep it ; Yes, overwrite" && p.footer() == "Keeps the saved journey",
          "S5 an occupied slot asks " + q(p.title()) + ", naming its save " + q(str(v.subtitle)) + ", No selected");
    const Card before = take_card();
    host_sd::reset_counters();
    h.key('\r');   // No
    const bool no_kept = p.title() == "Save Game" && p.footer() == "Slot 1 kept" && p.menu().selected_line == 0;
    h.key('\r');
    h.down();      // Yes highlighted
    h.mic();
    const bool mic_kept = h.rt->system_menu_open() && p.title() == "Save Game";
    h.key('\r');
    h.down();
    h.key('\b');   // Back
    const bool back_kept = h.rt->system_menu_open() && p.title() == "Save Game";
    check(no_kept && mic_kept && back_kept && take_card() == before && host_sd::counters().opens == 0,
          "S6 No, the Mic and Back (with Yes highlighted) each keep the slot: no file opened, every byte as it was");
    h.key('\r');
    h.down();
    h.key('\r');   // Yes
    const Card after = take_card();
    tdeck::AlphaSaveService cold;
    FrontendSaveCatalog cat{};
    cold.inspect_catalog(cat);
    check(!h.rt->system_menu_open() && p.last_line() == "Save complete: Slot 1" && slot_of(after, 1) == slot_of(before, 1) &&
              std::string(cat.slots[0].shown.name) == "Avery" && cat.slots[0].status == SaveSlotStatus::Saved,
          "S7 Yes saves into Slot 1 (now Avery), returns to the game; Slot 2 byte for byte");
    p.open_save();
    v = p.menu();
    check(v.selected_line == 0 && str(v.lines[0]) == "Slot 1  Avery     CURRENT" && str(v.lines[1]) == "Slot 2  Avery",
          "S8 CURRENT follows the save to Slot 1 (the journey's slot is where it last saved)");
    p.close();
    // A failed save keeps the menu and says what the slot still holds.
    p.mark();
    host_sd::faults().fail_write = "alpha1-s3";
    p.save(2);
    const std::string empty_fail = p.footer();
    const bool open_after = h.rt->system_menu_open();
    p.close();
    host_sd::faults().fail_write = "alpha1-g";
    p.save(0);
    const std::string held_fail = p.footer();
    host_sd::faults().fail_write.clear();
    p.close();
    check(open_after && empty_fail == "Save failed. Nothing was saved in Slot 3" && h.rt->system_menu_open() == false &&
              held_fail == "Save failed. Slot 1 keeps its last save" && p.saw("Save failed; prior kept"),
          "S9 a failed save stays in the menu and says so: " + q(empty_fail) + " / " + q(held_fail));
    // A failed save that changes what the slot lists: Slot 2 RECOVERED (its
    // newest refused), the save aimed at that refused generation, and its
    // commit rename failing -- the refused generation is left commit-less, so
    // the slot is now its older save, untagged. The menu that stays open must
    // list the card as it now is, not the RECOVERED it listed when it opened.
    p.save(1);                  // Slot 2's second generation (Slot 2 is the journey's again)
    tear(1, newest_gen(1));
    p.open_save();
    const std::string before_fail = str(p.menu().lines[1]);
    p.close();
    host_sd::faults().fail_rename = ".commit";
    p.save(1);                  // "Overwrite Slot 2?" -> Yes; the commit rename fails
    host_sd::faults().fail_rename.clear();
    const bool kept_open = h.rt->system_menu_open();
    const std::string why = p.footer();
    h.key('\r');                // the root's cursor is on Save Game
    const std::string row2 = str(p.menu().lines[1]);
    p.close();
    check(before_fail == "Slot 2  Avery     CURRENT, RECOVERED" && kept_open && why == "Save failed. Slot 2 keeps its last save" &&
              row2 == "Slot 2  Avery     CURRENT",
          "S10 after a failed save the open menu lists the card as it now is: " + q(before_fail) + " -> " + q(row2));
}

// ---- L: Load Game ---------------------------------------------------------------
void test_load(Run &h, Play &p) {
    std::printf("\nL  Load Game\n");
    // A card of its own: Slot 1 Kojac (101) then Avery (111); Slot 2 Iolo
    // (202) then Iolo (222); Slot 3 empty. All in Britannia.
    put_card({});
    p.leader("Kojac", 101);
    p.save(0);
    p.leader("Avery", 111);
    p.save(0);
    p.leader("Iolo", 202);
    p.save(1);
    p.leader("Iolo", 222);
    p.save(1);
    const Card base = take_card();
    p.leader("Zed", 999);
    p.mark();
    p.open_load();
    p.select(1);
    h.key('\r');
    check(!h.rt->system_menu_open() && p.gold() == 222 && p.last_line() == "Load complete" && !p.saw("Recovered") &&
              h.rt->save_service_for_test().last_slot() == 1,
          "L1 a healthy slot loads directly (Slot 2: gold 222), no extra page, no recovery line");
    p.open_load();
    auto v = p.menu();
    check(v.selected_line == 1 && page(v) == "Slot 1  Avery / Britannia ; Slot 2  Iolo      CURRENT / Britannia ; Slot 3  EMPTY",
          "L2 after Load Slot 2, Slot 2 alone is CURRENT: " + page(v));
    p.select(2);
    host_sd::reset_counters();
    h.key('\r');
    check(h.rt->system_menu_open() && p.footer() == "Slot 3 is empty" && p.title() == "Load Game" && p.gold() == 222 &&
              host_sd::counters().opens == 0,
          "L3 an empty slot refuses politely, stays on Load Game and opens no file");
    p.close();

    // Slot 1's newest (Avery 111) torn: listed RECOVERED with the save that loads.
    tear(0, newest_gen(0));
    p.leader("Zed", 999);
    p.mark();
    p.open_load();
    p.select(0);
    v = p.menu();
    dump("ui3-load-recovered");
    check(str(v.lines[0]) == "Slot 1  Kojac     RECOVERED" && str(v.details[0]) == "        Britannia" &&
              p.footer() == "Last save damaged; Enter loads the one before",
          "L4 the newest damaged: the slot is RECOVERED and shows the save that will load (Kojac, not Avery): " + page(v));
    h.key('\r');
    check(!h.rt->system_menu_open() && p.gold() == 101 && p.saw("Load complete") &&
              p.last_line() == "Recovered previous save (Slot 1)",
          "L5 Enter loads the save before (gold 101) and says so once: " + q(p.last_line()));
    // Both of Slot 1's generations torn: DAMAGED, neither EMPTY nor a journey.
    tear(0, 1 - newest_gen(0));   // the newest is still damaged from L4
    p.leader("Zed", 999);
    p.open_load();
    p.select(0);
    v = p.menu();
    dump("ui3-load-damaged");
    const std::string dd = p.footer();
    h.key('\r');
    check(str(v.lines[0]) == "Slot 1  DAMAGED   CURRENT" && str(v.details[0]) == "        Cannot be loaded" &&
              dd == "Damaged: this slot cannot be loaded" && p.footer() == "Slot 1 is damaged and cannot load" && p.gold() == 999,
          "L6 nothing loadable: DAMAGED (the journey's slot still marked), and Enter loads nothing: " + page(v));
    p.close();
    // A slot that fails as it loads: nothing else is loaded in its place.
    put_card(base);
    p.leader("Zed", 999);
    p.open_load();
    p.select(0);
    const std::string listed = str(p.menu().lines[0]);
    tear(0, 0);
    tear(0, 1);
    p.mark();
    h.key('\r');
    v = p.menu();
    check(listed.rfind("Slot 1  Avery", 0) == 0 && h.rt->system_menu_open() && p.gold() == 999 &&
              p.footer() == "Slot 1 could not be loaded. Nothing changed" && str(v.lines[0]).rfind("Slot 1  DAMAGED", 0) == 0 &&
              p.saw("No valid save"),
          "L7 a slot torn after the page opened: the load fails, no other slot is loaded (gold still 999), the page "
          "lists it DAMAGED and says nothing changed");
    p.close();
    put_card(base);
    g_healthy = base;
}

// ---- C: the title -------------------------------------------------------------
void test_title(Run &h, Play &p) {
    std::printf("\nC  the title\n");
    h.return_to_title();
    h.key(' ');
    dump("ui3-main-menu");
    const std::string menu0 = str(p.front().footer);
    h.key('j');
    auto v = p.front();
    dump("ui3-journey-onward");
    check(h.rt->frontend_state() == FrontendState::Continue && str(v.subtitle) == "Latest: Slot 2, Iolo, Britannia" &&
              str(v.footer) == "Enter continues Slot 2; Mic returns" && menu0 == "Select: arrows / Enter / J C T U A R S P", // A4-UI4: S, P named
          "C1 Journey Onward names Continue's slot and journey: " + q(str(v.subtitle)));
    h.down();
    h.key('\r');
    v = p.front();
    dump("ui3-title-load");
    check(h.rt->frontend_state() == FrontendState::Load && v.selected_line == 1 &&
              page(v) == "Slot 1  Avery / Britannia ; Slot 2  Iolo      LATEST / Britannia ; Slot 3  EMPTY",
          "C2 the title's Load Game marks Continue's slot LATEST and starts on it: " + page(v));
    h.mic();
    h.up();
    p.leader("Zed", 999);
    p.mark();
    h.key('\r');   // Continue
    check(!h.rt->frontend_open() && p.gold() == 222 && !p.saw("Recovered previous save"),
          "C3 Continue is one key and resumes Slot 2 (gold 222)");
    // Continue over a recovered slot says so.
    tear(1, newest_gen(1));
    h.return_to_title();
    h.key(' ');
    h.key('j');
    v = p.front();
    const std::string sub = str(v.subtitle), foot = str(v.footer);
    h.key('\r');
    check(sub == "Latest: Slot 2, Iolo, Britannia" && foot == "Latest save damaged; Enter loads the one before" &&
              p.gold() == 202 && p.last_line() == "Recovered previous save (Slot 2)",
          "C4 Continue on a recovered slot: named by the save that loads, then " + q(p.last_line()));
    // New Journey takes the empty slot, which becomes CURRENT.
    h.return_to_title();
    h.key(' ');
    p.key_quiet('c');
    for (const char *c = "Nova"; *c; ++c) p.key_quiet(uint8_t(*c));
    p.key_quiet('\r');
    p.key_quiet('f');
    for (int i = 0; i < 7; ++i) p.key_quiet(uint8_t((i & 1) ? 'b' : 'a'));
    full_roster(h);
    p.open_save();
    v = p.menu();
    check(!h.rt->frontend_open() && v.selected_line == 2 && str(v.lines[2]) == "Slot 3  Nova      CURRENT",
          "C5 a New Journey is saved in the empty Slot 3, which is then the CURRENT slot: " + page(v));
    p.close();
}

// ---- X: PC Save Transfer --------------------------------------------------------
void test_pc(Run &h, Play &p, const char *original) {
    std::printf("\nX  PC Save Transfer\n");
    std::string gam, ool;
    auto slurp = [](const fs::path &f, std::string &out) {
        std::ifstream in(f, std::ios::binary);
        out.assign(std::istreambuf_iterator<char>(in), {});
        return !out.empty();
    };
    const bool have = original && slurp(fs::path(original) / "SAVED.GAM", gam) && slurp(fs::path(original) / "SAVED.OOL", ool);
    if (have) {
        std::error_code ec;
        fs::create_directories(fs::path(host_sd::root()) / "sd/ultima5/import", ec);
        host_sd::write_card_file((std::string(kImport) + "/SAVED.GAM").c_str(), gam);
        host_sd::write_card_file((std::string(kImport) + "/SAVED.OOL").c_str(), ool);
    }
    h.return_to_title();
    h.key(' ');
    h.key('p');
    auto v = p.front();
    dump("ui3-pc-transfer");
    check(h.rt->frontend_state() == FrontendState::PcTransfer && page(v) == "Import the PC save into a slot ; Export a slot as a PC save",
          "X1 P reaches PC Save Transfer from the title (Import / Export): " + q(str(v.subtitle)));
    if (have) {
        h.key('\r');
        v = p.front();
        dump("ui3-pc-import");
        const bool rows = h.rt->frontend_state() == FrontendState::PcImportSlot && v.line_count == 3 && v.details[0] &&
                          str(v.lines[2]) == "Slot 3  Nova      LATEST";
        p.select(0, true);
        const Card before = take_card();
        h.key('\r');
        v = p.front();
        const bool asked = str(v.title) == "Replace Slot 1?" && v.selected_line == 0 && str(v.lines[0]) == "No, keep it";
        h.mic();
        check(rows && asked && str(p.front().footer) == "Import cancelled. Slot 1 is unchanged" && take_card() == before,
              "X2 Import lists the slot rows; an occupied slot asks \"Replace Slot 1?\" on No; the Mic cancels, every byte kept");
        h.mic();
    } else {
        check(true, "X2 (skipped: no original/u5/ultima5 SAVED.GAM + SAVED.OOL; a4_save3_pc_bridge_runtime covers import)");
    }
    h.down();
    h.key('\r');
    v = p.front();
    dump("ui3-pc-export");
    check(h.rt->frontend_state() == FrontendState::PcExportSlot && v.selected_line == 2 && str(v.lines[2]) == "Slot 3  Nova      LATEST" &&
              str(v.footer) == "Enter writes /ultima5/export/slot3",
          "X3 Export lists the same rows, on Continue's slot: " + page(v));
    h.mic();
    h.mic();
}

// ---- W: no new storage work -----------------------------------------------------
void test_warm(Run &h, Play &p) {
    std::printf("\nW  page opens\n");
    h.key('j');    // the title's Journey Onward -> Continue: back into the game
    h.key('\r');
    p.close();
    // A healthy card (a refused generation is re-read on every open by design,
    // A3-04G K6): the first open verifies what changed, the second is warm.
    put_card(g_healthy);
    p.open_save();
    p.close();
    host_sd::reset_counters();
    p.open_save();
    const auto c = host_sd::counters();
    p.close();
    check(c.bytes_read <= 6 * 64 && c.opens <= 6 + 1,
          "W1 a warm Save page open reads the six commit records alone (" + n(c.opens) + " opens, " + n(long(c.bytes_read)) + " B)");
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: a4_ui3_save_ux_runtime <pack> [original/u5/ultima5] [--dump <dir>]\n");
        return 2;
    }
    g_dump = arg_after(argc, argv, "--dump");
    const char *original = argc > 2 && std::strcmp(argv[2], "--dump") ? argv[2] : nullptr;
    if (!load_pack(argv[1])) {
        std::printf("RED the pack does not load -- the save/load checks cannot run\n");
        return 1;
    }
    const auto dir = fs::temp_directory_path() / "openu5-a4-ui3-card";
    host_sd::set_root(dir.string());
    put_card({});
    tdeck::AlphaSaveService::reserve_dma_headroom();
    {
        Run h(kParty);
        full_roster(h);
        Play p{h};
        test_save(h, p);
        test_load(h, p);
        test_title(h, p);
        test_pc(h, p, original);
        test_warm(h, p);
    }
    std::error_code ec;
    fs::remove_all(dir, ec);
    std::printf("A4-UI3 save UX runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
