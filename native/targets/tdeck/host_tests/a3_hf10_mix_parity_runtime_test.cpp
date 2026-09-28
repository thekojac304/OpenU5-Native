// Alpha 3 A3-HF10 (D-6 / D-70) -- Mix command parity: the player marks the
// reagents by hand, then answers "How much? ".
//
//   a3_hf10_mix_parity_runtime <openu5-alpha1-resources.bin> [--only <name>]
//
// The 1988 command (CMDS.OVL, derivation in re/notes/mix-hf10-command-parity.md):
//   0x1ad8 cmd_mix   no reagents at all -> "No reagents owned!"; spell; picker;
//                    quantity; n <= 0 -> silent; empty mask -> "Nothing to mix!";
//                    "Mixing..."; deduct n of every MARKED reagent; exact
//                    recipe -> "Done!" + n charges (cap 99), else the chest trap.
//   0x18be picker    owned reagents only, nothing marked; arrows move (clamped),
//                    RETURN / Space toggle, M mixes, ESC cancels, the rest is
//                    ignored.
//   0x1a70 quantity  "How much? " getnum(2); 0 -> abort; a marked reagent short
//                    of n (unsigned) -> "Insufficient reagents!" and ask again.
//   kernel 0x3b9e    getnum: 2 characters, a leading + / - allowed, ESC erases
//                    the buffer and keeps reading, backspace on empty is ignored.
//
// The device before HF10 skipped the picker (the recipe's own mask) and the
// quantity (1): alpha_runtime.cpp's Custom arm dispatched Mix at once.
//
// Everything goes through the REAL AlphaRuntime -- raw keys in -- drawing on
// the REAL tdeck_board.cpp over the fake ST7789, on the virtual clock. Only
// APIs that predate HF10 are used, so the file builds against HF9 (RED-first).
#include "../main/alpha_runtime.h"
#include "../main/idle_service.h"
#include "../main/tdeck_board.h"
#include "esp_timer.h"
#include "fake_tdeck_bus.h"
#include "openu5/hud.h"
#include "openu5/magic.h"

#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <utility>

using namespace openu5;
using tdeck::RawInputKind;
namespace bus = openu5_host_bus;

namespace tdeck {
void host_memory_save_forget_for_test();
void host_memory_save_damage_for_test();
} // namespace tdeck

namespace {
int checks = 0, failures = 0;
bool check(bool good, const char *id, const std::string &label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s %s\n", good ? "GREEN" : "RED", id, label.c_str());
    return good;
}
std::string n(long long v) { return std::to_string(v); }
const tdeck::AlphaResourceOwners *pack = nullptr;
constexpr int kW = 320;
constexpr int64_t kFrameMs = 5;
// Reagent ids (DATA.OVL order) and the spells used below, from magic_tables.inc.
enum Reagent { Ash = 0, Ginseng = 1, Garlic = 2, Silk = 3, Moss = 4, Pearl = 5, Shade = 6, Mandrake = 7 };
constexpr int kInLor = 0; // Sulfur Ash
constexpr int kAnNox = 3; // Ginseng + Garlic
constexpr int kMani = 4;  // Ginseng + Spider Silk

struct Run {
    std::unique_ptr<tdeck::AlphaRuntime> rt = std::make_unique<tdeck::AlphaRuntime>();
    tdeck::Board board{};
    tdeck::IdleService idle{};
    size_t mark = 0;
    Run() {
        openu5_host_virtual_clock_us() = 5'000'000;
        bus::install();
        bus::model() = bus::Model{};
        bus::model().timed = false;
        board.initialize_display();
        idle.attach(bus::idle_passes());
        board.set_idle_service(&idle);
        rt->attach_idle_service(&idle);
        tdeck::host_memory_save_forget_for_test();
        tdeck::AlphaRuntime::HostTestFixture f{};
        f.world = pack->world;
        f.npc_locations = pack->npc_locations;
        f.pack = pack;
        f.location_x = pack->location_x;
        f.location_y = pack->location_y;
        f.location_count = pack->location_count;
        f.render_pixels = true;
        f.patterned_test_tiles = true;
        rt->attach_host_test_fixture(f);
        auto &g = rt->game();
        g.time.hour = 12;
        g.time.minute = 0;
        g.food = 900;
        g.gold = 100;
        for (auto &q : g.spell_quantities) q = 0; // a new game carries some mixed spells
        const char *names[] = {"Avatar", "Shamino"};
        g.party.character_count = g.party.party_size = 2;
        g.party.active_character = 255;
        for (int i = 0; i < 2; ++i) {
            auto &m = g.party.characters[i];
            std::snprintf(m.name, sizeof(m.name), "%s", names[i]);
            m.status = 'G';
            m.character_class = i ? 'F' : 'A';
            m.level = 1;
            m.current_hp = m.max_hp = 300;
            m.strength = m.dexterity = m.intelligence = 20;
        }
        // Britannia on foot, in open grassland (the Developer's own surface).
        g.position.map = {0, 0};
        g.position.xy = {90, 100};
        frames(3);
    }
    GameState &g() { return rt->game(); }
    const UiSession &ui() { return *rt->ui(); }
    UiMode mode() { return ui().mode(); }
    std::string prompt() { return ui().prompt(); }
    void frames(int k) {
        for (int i = 0; i < k; ++i) {
            openu5_host_virtual_clock_us() += kFrameMs * 1000;
            rt->render(board);
        }
    }
    void raw(tdeck::RawInputEvent e) {
        e.timestamp_us = openu5_host_virtual_clock_us();
        rt->handle(e);
        frames(1);
    }
    void key(uint8_t code, bool alt = false) {
        tdeck::RawInputEvent e{};
        e.kind = RawInputKind::Keyboard;
        e.transition = tdeck::KeyTransition::Pressed;
        e.code = e.base_code = code;
        e.modifiers.alt = alt;
        raw(e);
    }
    void type(const char *s) { for (; *s; ++s) key(uint8_t(*s)); }
    void mic() {
        tdeck::RawInputEvent e{};
        e.kind = RawInputKind::Keyboard;
        e.column = tdeck::kMicrophoneKeyColumn;
        e.row = tdeck::kMicrophoneKeyRow;
        e.transition = tdeck::KeyTransition::Pressed;
        raw(e);
        e.transition = tdeck::KeyTransition::Released;
        raw(e);
    }
    void ball(RawInputKind k) {
        openu5_host_virtual_clock_us() += 100000; // past the trackball debounce
        tdeck::RawInputEvent e{};
        e.kind = k;
        raw(e);
    }
    void up() { ball(RawInputKind::TrackballUp); }
    void down() { ball(RawInputKind::TrackballDown); }
    void left() { ball(RawInputKind::TrackballLeft); }
    void right() { ball(RawInputKind::TrackballRight); }

    void set_mark() { mark = ui().transcript_size(); }
    std::string since_mark() {
        std::string out;
        for (size_t i = mark; i < ui().transcript_size(); ++i)
            if (const auto *b = ui().transcript_at(i)) { out += b->text; out += '|'; }
        return out;
    }
    bool shown(const char *needle) { return since_mark().find(needle) != std::string::npos; }
    size_t new_blocks() { return ui().transcript_size() - mark; }

    void reagents(std::initializer_list<std::pair<int, int>> counts) {
        for (auto &r : g().reagent_quantities) r = 0;
        for (const auto &c : counts) g().reagent_quantities[c.first] = c.second;
    }
    // --- the command, key by key ------------------------------------------
    /** 'm', then the spell list's row `spell` (rows are spell ids 0..47). */
    void choose_spell(int spell) {
        key('m');
        for (int i = 0; i < spell; ++i) down();
        key('\r');
    }
    bool in_picker() {
        UiSelectionView v{};
        return ui().selection_view(v) && prompt() == "Reagents:";
    }
    size_t picker_rows() {
        UiSelectionView v{};
        return ui().selection_view(v) ? v.count : 0;
    }
    size_t picker_cursor() {
        UiSelectionView v{};
        return ui().selection_view(v) ? v.cursor : size_t(-1);
    }
    /** Row of reagent `r` in the picker: owned reagents in id order. */
    int row_of(int r) {
        int row = 0;
        for (int i = 0; i < r; ++i) row += g().reagent_quantities[i] > 0;
        return row;
    }
    /** Bounded: with no picker open (the HF9 device) it gives up, never spins. */
    void cursor_to(int row) {
        for (int i = 0; i < 16 && in_picker() && int(picker_cursor()) < row; ++i) down();
        for (int i = 0; i < 16 && in_picker() && int(picker_cursor()) > row; ++i) up();
    }
    void toggle(int reagent, bool space = false) {
        cursor_to(row_of(reagent));
        key(space ? ' ' : '\r');
    }
    bool in_quantity() { return mode() == UiMode::NumericEntry && prompt() == "How much? "; }
    /** Full command: spell, marks, M, quantity, RETURN. */
    void mix(int spell, std::initializer_list<int> marks, const char *qty) {
        choose_spell(spell);
        for (int r : marks) toggle(r);
        key('m');
        type(qty);
        key('\r');
    }

    // --- the real Board's GRAM ----------------------------------------------
    /** A 5x7 glyph cell of the selector list: row `row`, text column `col`. */
    static bool selector_cell_lit(int row, int col) {
        const uint16_t *p = bus::gram();
        const int x0 = 184 + col * 6, y0 = 46 + row * 14;
        for (int y = y0; y < y0 + 7; ++y)
            for (int x = x0; x < x0 + 5; ++x)
                if (p[y * kW + x] != 0) return true;
        return false;
    }
    static uint64_t cell_hash(int row, int col) {
        const uint16_t *p = bus::gram();
        const int x0 = 184 + col * 6, y0 = 46 + row * 14;
        uint64_t h = 1469598103934665603ull;
        for (int y = y0; y < y0 + 7; ++y)
            for (int x = x0; x < x0 + 5; ++x) h = (h ^ p[y * kW + x]) * 1099511628211ull;
        return h;
    }
    static bool region_lit(int x0, int y0, int w, int h) {
        const uint16_t *p = bus::gram();
        for (int y = y0; y < y0 + h; ++y)
            for (int x = x0; x < x0 + w; ++x)
                if (p[y * kW + x] != 0) return true;
        return false;
    }
    /** The gameplay context bar's status row (prompt + typed digits). */
    static uint64_t context_status_hash() {
        const uint16_t *p = bus::gram();
        uint64_t h = 1469598103934665603ull;
        for (int y = tdeck::kContextBarStatusY; y < tdeck::kContextBarStatusY + tdeck::kContextBarStatusH; ++y)
            for (int x = kHudPartyFrameX; x < kW; ++x) h = (h ^ p[y * kW + x]) * 1099511628211ull;
        return h;
    }
};

struct Snapshot {
    int32_t reagents[8]{}, spells[48]{};
    WorldPosition pos{};
    int32_t minute = 0;
    explicit Snapshot(const GameState &g) {
        std::memcpy(reagents, g.reagent_quantities, sizeof reagents);
        std::memcpy(spells, g.spell_quantities, sizeof spells);
        pos = g.position;
        minute = g.time.hour * 60 + g.time.minute;
    }
    bool inventory_same(const GameState &g) const {
        return std::memcmp(reagents, g.reagent_quantities, sizeof reagents) == 0 &&
               std::memcmp(spells, g.spell_quantities, sizeof spells) == 0;
    }
    bool position_same(const GameState &g) const {
        return pos.map.location == g.position.map.location && pos.map.floor == g.position.map.floor &&
               pos.xy.x == g.position.xy.x && pos.xy.y == g.position.xy.y;
    }
};
bool trap_text(Run &h) {
    return h.shown("ACID") || h.shown("POISON") || h.shown("BOMB") || h.shown("GAS");
}

// ---- N1: the picker appears; nothing is chosen for the player -------------
void test_picker() {
    Run h;
    h.reagents({{Ash, 5}, {Ginseng, 4}, {Silk, 3}, {Mandrake, 2}});
    const Snapshot before(h.g());
    h.set_mark();
    h.choose_spell(kInLor);
    check(h.in_picker(), "N1.1", "** choosing a spell opens the reagent picker (\"Reagents:\") **");
    check(before.inventory_same(h.g()) && !h.shown("Mixing") && !h.shown("Done"), "N1.2",
          "** nothing is mixed or deducted by choosing the spell (no auto-recipe) **");
    check(h.picker_rows() == 4 && h.picker_cursor() == 0, "N1.3",
          "the rows are the four OWNED reagents, cursor on the first (0x18ca-0x18dd)");
    bool none_marked = true;
    for (int row = 0; row < 4; ++row) none_marked = none_marked && !Run::selector_cell_lit(row, 4);
    check(h.in_picker() && Run::region_lit(184, 5, 134, 14) && none_marked && Run::selector_cell_lit(0, 0), "N1.4",
          "real Board: the title and list are drawn, the cursor '>' is on row 0, NO row carries a mark");
    h.toggle(Silk);
    check(h.in_picker() && Run::selector_cell_lit(h.row_of(Silk), 4) && !Run::selector_cell_lit(h.row_of(Ash), 4) &&
              Run::selector_cell_lit(h.row_of(Silk), 0),
          "N1.5", "real Board: RETURN marks the row under the cursor, and only that row");
    h.toggle(Silk, true);
    check(h.in_picker() && !Run::selector_cell_lit(h.row_of(Silk), 4), "N1.6",
          "Space toggles the same row back off; the picker stays open (0x19ee)");
    check(before.inventory_same(h.g()), "N1.7", "marking and unmarking mutate nothing");
    h.mic();
}

// ---- N2: the exact recipe -------------------------------------------------
void test_correct() {
    Run h;
    h.reagents({{Ash, 5}, {Ginseng, 4}, {Garlic, 3}});
    h.g().spell_quantities[kInLor] = 2;
    h.set_mark();
    h.mix(kInLor, {Ash}, "1");
    check(h.g().reagent_quantities[Ash] == 4 && h.g().reagent_quantities[Ginseng] == 4 &&
              h.g().reagent_quantities[Garlic] == 3 && h.g().spell_quantities[kInLor] == 3,
          "N2.1", "In Lor with Sulfur Ash marked, 1: Ash 5->4, the others untouched, In Lor 2->3");
    const auto text = h.since_mark();
    const auto mixing = text.find("Mixing..."), done = text.find("Done!");
    check(mixing != std::string::npos && done != std::string::npos && mixing < done && !trap_text(h), "N2.2",
          "\"Mixing...\" then \"Done!\", no trap");
    h.set_mark();
    h.mix(kAnNox, {Garlic, Ginseng}, "2");
    check(h.g().reagent_quantities[Ginseng] == 2 && h.g().reagent_quantities[Garlic] == 1 &&
              h.g().spell_quantities[kAnNox] == 2 && h.shown("Done!"),
          "N2.3", "a two-reagent recipe marked in either order is exact: An Nox +2, Ginseng/Garlic -2");
    check(h.mode() == UiMode::Exploration, "N2.4", "the command ends back in Explore");
}

// ---- N3: a wrong recipe ---------------------------------------------------
void test_wrong() {
    {
        Run h;
        h.reagents({{Ash, 5}, {Ginseng, 4}, {Garlic, 3}});
        const Snapshot before(h.g());
        h.set_mark();
        h.mix(kInLor, {Ginseng}, "1");
        check(h.g().reagent_quantities[Ginseng] == 3 && h.g().reagent_quantities[Ash] == 5 &&
                  h.g().reagent_quantities[Garlic] == 3,
              "N3.1", "the WRONG reagent marked: it is spent anyway (0x1baa), the recipe's own is not touched");
        check(std::memcmp(before.spells, h.g().spell_quantities, sizeof before.spells) == 0, "N3.2",
              "no spell is charged");
        check(h.shown("Mixing...") && !h.shown("Done!") && trap_text(h), "N3.3",
              "\"Mixing...\", no \"Done!\", and the chest trap fires (0x1bf6-0x1c04)");
    }
    {
        Run h;
        h.reagents({{Ash, 5}, {Ginseng, 4}});
        h.set_mark();
        h.mix(kInLor, {Ash, Ginseng}, "1");
        check(h.g().reagent_quantities[Ash] == 4 && h.g().reagent_quantities[Ginseng] == 3 &&
                  h.g().spell_quantities[kInLor] == 0 && trap_text(h),
              "N3.4", "an EXTRA reagent is a wrong set too: both spent, nothing charged, the trap");
    }
    {
        Run h;
        h.reagents({{Ginseng, 4}, {Silk, 4}});
        h.set_mark();
        h.mix(kMani, {Ginseng}, "1");
        check(h.g().reagent_quantities[Ginseng] == 3 && h.g().reagent_quantities[Silk] == 4 &&
                  h.g().spell_quantities[kMani] == 0 && trap_text(h),
              "N3.5", "a MISSING mark is a wrong set: Mani with only Ginseng marked explodes");
    }
}

// ---- N4: a reagent the party does not own -----------------------------------
void test_missing() {
    {
        Run h;
        h.reagents({{Ginseng, 4}, {Garlic, 2}}); // Mani needs Spider Silk: none
        h.choose_spell(kMani);
        check(h.in_picker() && h.picker_rows() == 2, "N4.1",
              "Mani without Spider Silk still opens the picker; Spider Silk is simply not a row");
        h.toggle(Ginseng);
        h.key('m');
        h.set_mark();
        h.type("1");
        h.key('\r');
        check(h.g().reagent_quantities[Ginseng] == 3 && h.g().spell_quantities[kMani] == 0 && trap_text(h), "N4.2",
              "mixing without it is a wrong set: Ginseng spent, no charge, the trap (no ownership pre-check)");
    }
    {
        Run h;
        h.reagents({});
        const Snapshot before(h.g());
        h.set_mark();
        h.key('m');
        check(h.shown("No reagents owned!") && h.mode() == UiMode::Exploration && before.inventory_same(h.g()),
              "N4.3", "no reagent at all: \"No reagents owned!\" and nothing opens (0x1af8)");
    }
}

// ---- N5: the quantity question ----------------------------------------------
void test_prompt() {
    Run h;
    h.reagents({{Ash, 5}});
    h.choose_spell(kInLor);
    h.toggle(Ash);
    const uint64_t picker_status = Run::context_status_hash();
    const Snapshot before(h.g());
    h.key('m');
    check(h.in_quantity(), "N5.1", "** M closes the picker and asks \"How much? \" (0x1a7d) **");
    check(before.inventory_same(h.g()), "N5.2", "nothing is deducted before the answer");
    const uint64_t asked = Run::context_status_hash();
    h.key('3');
    const uint64_t typed = Run::context_status_hash();
    check(asked != picker_status && typed != asked && h.ui().input_length() == 1, "N5.3",
          "real Board: the question reaches the context bar, and the typed digit follows it");
    h.set_mark();
    h.key('\r');
    check(h.g().reagent_quantities[Ash] == 2 && h.g().spell_quantities[kInLor] == 3, "N5.4",
          "the answer is the quantity");
}

// ---- N6: more than one ----------------------------------------------------
void test_many() {
    {
        Run h;
        h.reagents({{Ginseng, 9}, {Silk, 8}, {Ash, 5}});
        h.g().spell_quantities[kMani] = 1;
        h.set_mark();
        h.mix(kMani, {Ginseng, Silk}, "3");
        check(h.g().reagent_quantities[Ginseng] == 6 && h.g().reagent_quantities[Silk] == 5 &&
                  h.g().reagent_quantities[Ash] == 5,
              "N6.1", "** Mani x3: Ginseng 9->6 and Spider Silk 8->5 (3 x recipe), Ash untouched **");
        check(h.g().spell_quantities[kMani] == 4 && h.shown("Done!"), "N6.2", "** Mani 1->4 (+3) **");
    }
    {
        Run h;
        h.reagents({{Ash, 20}});
        h.mix(kInLor, {Ash}, "12");
        check(h.g().reagent_quantities[Ash] == 8 && h.g().spell_quantities[kInLor] == 12, "N6.3",
              "two digits: 12 -> Ash 20->8, In Lor +12 (getnum(2))");
    }
    {
        Run h;
        h.reagents({{Ash, 99}});
        h.mix(kInLor, {Ash}, "123");
        check(h.g().reagent_quantities[Ash] == 87 && h.g().spell_quantities[kInLor] == 12, "N6.4",
              "a third digit is ignored, not clamped: \"123\" is 12");
    }
    {
        // Real Board: the count the picker shows is the one left after the mix.
        Run h;
        h.reagents({{Ash, 5}});
        h.choose_spell(kInLor);
        const uint64_t tens = Run::cell_hash(0, 1), units = Run::cell_hash(0, 2);
        h.toggle(Ash);
        h.key('m');
        h.type("3");
        h.key('\r');
        h.choose_spell(kInLor);
        check(h.in_picker() && Run::cell_hash(0, 1) == tens && Run::cell_hash(0, 2) != units, "N6.5",
              "real Board: reopening the picker shows the Sulfur Ash count 05 -> 02 (units cell redrawn)");
        h.mic();
    }
}

// ---- N7: more than the marked reagents allow ---------------------------------
void test_short() {
    Run h;
    h.reagents({{Ginseng, 2}, {Silk, 8}});
    h.choose_spell(kMani);
    h.toggle(Ginseng);
    h.toggle(Silk);
    h.key('m');
    const Snapshot before(h.g());
    h.set_mark();
    h.type("3");
    h.key('\r');
    check(h.shown("Insufficient reagents!") && h.in_quantity() && h.ui().input_length() == 0, "N7.1",
          "** 3 with Ginseng 2: \"Insufficient reagents!\" and \"How much? \" again (0x1ac6) **");
    check(before.inventory_same(h.g()) && !h.shown("Mixing"), "N7.2", "** nothing deducted, nothing charged **");
    h.set_mark();
    h.type("2");
    h.key('\r');
    check(h.g().reagent_quantities[Ginseng] == 0 && h.g().reagent_quantities[Silk] == 6 &&
              h.g().spell_quantities[kMani] == 2 && h.shown("Done!"),
          "N7.3", "the re-asked answer mixes: 2 -> Ginseng 2->0, Silk 8->6, Mani +2");
    {
        Run k;
        k.reagents({{Ash, 3}, {Ginseng, 1}});
        k.set_mark();
        k.mix(kInLor, {Ash}, "3");
        check(k.g().reagent_quantities[Ash] == 0 && k.g().spell_quantities[kInLor] == 3 &&
                  !k.shown("Insufficient"),
              "N7.4", "only MARKED reagents are tested: an unmarked short reagent does not refuse");
    }
}

// ---- N8: zero, empty, sign, letters -----------------------------------------
void test_zero() {
    {
        Run h;
        h.reagents({{Ash, 5}});
        const Snapshot before(h.g());
        h.set_mark();
        h.mix(kInLor, {Ash}, "0");
        check(before.inventory_same(h.g()) && h.mode() == UiMode::Exploration && !h.shown("Mixing") &&
                  !h.shown("Insufficient") && !h.shown("Nothing"),
              "N8.1", "0: a silent abort, nothing mutated (0x1a8e / 0x1b71)");
        h.set_mark();
        h.mix(kInLor, {Ash}, "");
        check(before.inventory_same(h.g()) && h.mode() == UiMode::Exploration && !h.shown("Mixing"), "N8.2",
              "RETURN on an empty answer: the same silent abort");
    }
    {
        Run h;
        h.reagents({{Ash, 5}});
        h.choose_spell(kInLor);
        h.toggle(Ash);
        h.key('m');
        h.type("x?a");
        check(h.in_quantity() && h.ui().input_length() == 0, "N8.3", "letters are not digits: ignored");
        h.set_mark();
        h.type("-5");
        h.key('\r');
        check(h.shown("Insufficient reagents!") && h.in_quantity() && h.g().reagent_quantities[Ash] == 5, "N8.4",
              "\"-5\" with a reagent marked: unsigned 0xfffb > count -> \"Insufficient reagents!\", asked again");
        h.set_mark();
        h.type("+3");
        h.key('\r');
        check(h.g().reagent_quantities[Ash] == 2 && h.g().spell_quantities[kInLor] == 3 && h.shown("Done!"), "N8.5",
              "\"+3\" is 3 (kernel 0x3be2 accepts a leading sign)");
    }
    {
        Run h;
        h.reagents({{Ash, 5}});
        const Snapshot before(h.g());
        h.choose_spell(kInLor);
        h.key('m'); // nothing marked
        check(h.in_quantity(), "N8.6", "M with nothing marked still asks \"How much? \" (0x1a70 before 0x1b78)");
        h.set_mark();
        h.type("2");
        h.key('\r');
        check(h.shown("Nothing to mix!") && !h.shown("Mixing") && before.inventory_same(h.g()), "N8.7",
              "then \"Nothing to mix!\", nothing mutated");
        h.set_mark();
        h.choose_spell(kInLor);
        h.key('m');
        h.type("-2");
        h.key('\r');
        check(!h.shown("Nothing") && !h.shown("Insufficient") && before.inventory_same(h.g()) &&
                  h.mode() == UiMode::Exploration,
              "N8.8", "an empty mask with -2: returned as is, then n <= 0 aborts in silence");
    }
}

// ---- N9: cancel at the picker -----------------------------------------------
void test_cancel_picker() {
    Run h;
    h.reagents({{Ash, 5}, {Ginseng, 4}});
    const Snapshot before(h.g());
    h.choose_spell(kInLor);
    h.toggle(Ash);
    h.key('\b');
    check(h.in_picker(), "N9.1", "backspace is not ESC in the picker: ignored (0x1a50)");
    h.set_mark();
    h.mic();
    check(h.mode() == UiMode::Exploration && before.inventory_same(h.g()) && !h.shown("Mixing") &&
              !h.shown("None"),
          "N9.2", "** ESC (Mic) closes it: nothing mutated, no message (0x1a2e -> -1) **");
    h.choose_spell(kInLor);
    check(h.in_picker() && !Run::selector_cell_lit(0, 4) && !Run::selector_cell_lit(1, 4), "N9.3",
          "reopened, nothing is marked: the marks died with the picker");
    h.mic();
}

// ---- N10: leaving the quantity question -------------------------------------
void test_cancel_quantity() {
    Run h;
    h.reagents({{Ash, 5}});
    const Snapshot before(h.g());
    h.choose_spell(kInLor);
    h.toggle(Ash);
    h.key('m');
    h.type("4");
    h.mic();
    check(h.in_quantity() && h.ui().input_length() == 0, "N10.1",
          "ESC (Mic) erases the typed answer and keeps asking (kernel 0x3c0e)");
    h.mic();
    check(h.in_quantity(), "N10.2", "ESC on an empty answer is ignored: getnum has no cancel");
    h.type("45");
    h.key('\b');
    check(h.in_quantity() && h.ui().input_length() == 1, "N10.3", "backspace deletes one digit");
    h.key('\b');
    h.key('\b');
    check(h.in_quantity() && h.ui().input_length() == 0, "N10.4", "backspace on an empty answer is ignored");
    h.set_mark();
    h.key('\r');
    check(h.mode() == UiMode::Exploration && before.inventory_same(h.g()) && !h.shown("Mixing"), "N10.5",
          "** leaving it (RETURN on nothing) mutates nothing **");
}

// ---- N11: the 99 cap --------------------------------------------------------
void test_cap() {
    Run h;
    h.reagents({{Ash, 30}});
    h.g().spell_quantities[kInLor] = 97;
    h.mix(kInLor, {Ash}, "5");
    check(h.g().spell_quantities[kInLor] == 99 && h.g().reagent_quantities[Ash] == 25, "N11.1",
          "97 + 5 caps at 99 (0x1be7), and all 5 reagents are spent");
    h.mix(kInLor, {Ash}, "1");
    check(h.g().spell_quantities[kInLor] == 99 && h.g().reagent_quantities[Ash] == 24, "N11.2",
          "at 99 a further mix still spends its reagents and stays at 99");
}

// ---- N12: keys stay inside the command ---------------------------------------
void test_leak() {
    Run h;
    h.reagents({{Ash, 5}, {Ginseng, 4}, {Garlic, 3}});
    const Snapshot before(h.g());
    h.choose_spell(kInLor);
    h.set_mark();
    h.up();
    check(h.picker_cursor() == 0, "N12.1", "up on the first row stays there (clamped, 0x19b2)");
    h.cursor_to(2);
    h.down();
    h.right();
    check(h.picker_cursor() == 2, "N12.2", "down/right on the last row stay there (clamped, 0x19d0)");
    h.left();
    check(h.picker_cursor() == 1, "N12.3", "left moves up like up (getkey 3)");
    h.type("1lkgw5");
    check(h.in_picker() && h.new_blocks() == 0 && before.position_same(h.g()) && before.inventory_same(h.g()),
          "N12.4", "digits and command letters in the picker: swallowed, no echo, no move, no command");
    h.cursor_to(0);
    h.key('\r');
    h.key('m');
    h.set_mark();
    h.type("lk");
    h.up();
    check(h.in_quantity() && h.new_blocks() == 0 && before.position_same(h.g()), "N12.5",
          "letters and the trackball at \"How much? \": swallowed");
    h.type("1");
    h.key('\r');
    check(h.g().spell_quantities[kInLor] == 1 && h.mode() == UiMode::Exploration, "N12.6", "precondition: mixed");
    const int32_t minute = h.g().time.hour * 60 + h.g().time.minute;
    h.set_mark();
    h.key('m');
    check(h.mode() == UiMode::SpellSelection, "N12.7", "the next 'm' is a fresh Mix, not a leftover");
    h.mic();
    check(h.mode() == UiMode::Exploration && h.g().time.hour * 60 + h.g().time.minute == minute, "N12.8",
          "and Mic leaves it; no turn was spent");
}

// ---- N13: a load in the middle ----------------------------------------------
void test_load() {
    for (int stage = 0; stage < 2; ++stage) {
        Run h;
        h.reagents({{Ash, 5}});
        h.key('s', true); // Alt+S: the generation to go back to
        h.reagents({{Ash, 7}});
        h.choose_spell(kInLor);
        h.toggle(Ash);
        if (stage) {
            h.key('m');
            h.type("2");
        }
        const char *where = stage ? "\"How much? \"" : "the picker";
        h.set_mark();
        h.key('l', true); // Alt+L
        const auto p = h.rt->transient_probe_for_test();
        check(h.shown("Load complete") && h.mode() == UiMode::Exploration && h.prompt().empty() &&
                  h.ui().input_length() == 0 && !p.parked_pick && h.g().reagent_quantities[Ash] == 5,
              stage ? "N13.2" : "N13.1", std::string("** a load at ") + where + " leaves no Mix state behind **");
        h.set_mark();
        h.type("2");
        h.key('\r');
        check(h.g().reagent_quantities[Ash] == 5 && h.g().spell_quantities[kInLor] == 0 && !h.shown("Mixing") &&
                  !h.shown("Insufficient"),
              stage ? "N13.4" : "N13.3", "the keys after it reach Explore; nothing mixes from the old question");
        h.choose_spell(kInLor);
        check(h.in_picker() && !Run::selector_cell_lit(0, 4), stage ? "N13.6" : "N13.5",
              "Mix opens afresh with nothing marked");
        h.mic();
    }
    {
        Run h; // nothing on the card
        h.reagents({{Ash, 5}});
        h.choose_spell(kInLor);
        h.toggle(Ash);
        h.key('m');
        h.set_mark();
        h.key('l', true);
        check(h.shown("No valid save") && h.in_quantity(), "N13.7",
              "a FAILED load leaves the question exactly where it was");
        h.type("2");
        h.key('\r');
        check(h.g().reagent_quantities[Ash] == 3 && h.g().spell_quantities[kInLor] == 2, "N13.8",
              "and its answer still mixes with the marks made before");
    }
}

// ---- N14: neighbours unchanged -----------------------------------------------
void test_regression() {
    Run h;
    h.reagents({{Ash, 5}});
    h.g().spell_quantities[kInLor] = 2;
    h.g().party.characters[0].current_mp = 20;
    h.key('c');
    if (h.mode() == UiMode::PartySelection) h.key('\r'); // "Player: " (kernel 0x4988)
    check(h.mode() == UiMode::SpellSelection && h.prompt() == "Spell", "N14.1",
          "Cast still opens its spell list");
    h.key('\r');
    check(h.g().spell_quantities[kInLor] == 1 && h.g().reagent_quantities[Ash] == 5, "N14.2",
          "casting In Lor spends one charge and no reagent");
    h.key('u');
    UiSelectionView v{};
    const bool use_open = h.ui().selection_view(v) && h.mode() == UiMode::InventorySelection;
    h.mic();
    check(use_open && h.mode() == UiMode::Exploration, "N14.3", "the (U)se picker opens and Mic still closes it");
    h.key('r');
    const bool ready_open = h.ui().selection_view(v);
    const size_t first = v.cursor;
    if (ready_open && h.mode() == UiMode::PartySelection) {
        h.up();
        h.ui().selection_view(v);
    }
    const bool wraps = !ready_open || h.mode() != UiMode::PartySelection || v.cursor != first;
    h.mic();
    check(ready_open && wraps && h.mode() == UiMode::Exploration, "N14.4",
          "other pickers keep their wrap-around cursor (Ready whom?: up from the first row wraps)");
    h.key('h'); // Hole up: the generic numeric entry
    const bool hours = h.mode() == UiMode::NumericEntry;
    h.mic();
    check(hours && h.mode() == UiMode::Exploration, "N14.5",
          "a non-Mix numeric prompt still cancels on Mic (the ESC-erases rule is Mix's getnum only)");
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <openu5-alpha1-resources.bin> [--only <name>]\n", argv[0]);
        return 2;
    }
    tdeck::AlphaResourcePack source;
    tdeck::AlphaResourceReport report{};
    if (source.open(argv[1], report) != ESP_OK) return 2;
    static tdeck::AlphaResourceOwners owners{};
    if (source.load(owners, report) != ESP_OK) return 2;
    pack = &owners;
    const char *only = argc > 3 && std::strcmp(argv[2], "--only") == 0 ? argv[3] : nullptr;
    const std::pair<const char *, std::function<void()>> tests[] = {
        {"picker", test_picker},
        {"correct", test_correct},
        {"wrong", test_wrong},
        {"missing", test_missing},
        {"prompt", test_prompt},
        {"many", test_many},
        {"short", test_short},
        {"zero", test_zero},
        {"cancel-picker", test_cancel_picker},
        {"cancel-quantity", test_cancel_quantity},
        {"cap", test_cap},
        {"leak", test_leak},
        {"load", test_load},
        {"regression", test_regression},
    };
    for (const auto &t : tests)
        if (!only || std::strcmp(only, t.first) == 0) t.second();
    std::printf("\na3_hf10_mix_parity_runtime: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
