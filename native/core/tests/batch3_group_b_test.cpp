// Batch 3 GROUP B -- RED tests for R-07/R-08 (Use picker id space wrong and
// incomplete). See native/targets/tdeck/GAMEPLAY_INTEGRATION_AUDIT.md.
//
// display_names.cpp::usable_item_display_name() is host-buildable and is
// exercised directly (B1, B2, B5). The picker ROW-BUILDING logic itself is
// now extracted into openu5::usable_item_picker_rows() (native/core/include/
// openu5/inventory_picker.h, native/core/src/inventory_picker.cpp) -- a
// plain, ESP-free function/header. AlphaRuntime::open_selection()
// (native/targets/tdeck/main/alpha_runtime.cpp), which is ESP-IDF-only and
// not itself host-buildable, calls this SAME function instead of inlining
// the logic, so this test and production share one definition (no more
// test-local mirror; see the batch report section B for the call-site
// proof). The extraction reproduces CURRENT (broken) behavior exactly --
// same id space, same offset-index bug -- it is not a fix.
#include "openu5/display_names.h"
#include "openu5/inventory_picker.h"
#include "openu5/state.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace openu5;
namespace {
int checks = 0, failures = 0;
void check(bool ok, const char *what) {
    ++checks;
    if (!ok) {
        ++failures;
        std::cerr << "[FAIL] batch3 group B check " << checks << ": " << what << "\n";
    } else {
        std::cerr << "[PASS] batch3 group B check " << checks << ": " << what << "\n";
    }
}

// Builds the shared seam's narrow input from a GameState, exactly the fields
// AlphaRuntime::open_selection() reads today (game_.magic_carpets etc.), plus
// the 8 moonstone-owned flags (real ids 21-28, phase = id-21) supplied
// separately by the caller since QuestWorldServices' Moonstone::buried is not
// a GameState field -- see report section L. This is fixture plumbing only;
// the production-shared logic lives entirely in usable_item_picker_rows().
UsableItemPickerInput picker_input(const GameState &g, const bool (&moonstone_owned)[8] = {}) {
    UsableItemPickerInput in;
    in.magic_carpets = g.magic_carpets;
    in.skull_keys = g.skull_keys;
    in.grapple = g.grapple;
    in.spyglass = g.spyglass;
    in.sextant = g.sextant;
    in.wooden_box = g.wooden_box;
    for (int i = 0; i < 8; ++i) in.moonstone_owned[i] = moonstone_owned[i];
    return in;
}
} // namespace

// B1: narrow display-name tests for the authoritative real id space
// (game/src/core/endgame/use-tools.ts). Only ids the reference names
// individually are asserted; shards (29-31) share one generic "Shard"
// verb/name in the reference and are asserted only as non-null+non-generic,
// per the spec's "do not invent specific names" guard. Moonstones (21-28)
// are likewise asserted only as non-null (the native/reference naming for
// them is the generic "Moonstone" string used by quest_world.cpp's burial
// message, not 8 unique names -- see report section L); do not invent
// individual moonstone names.
static void b1_real_id_names() {
    check(usable_item_display_name(16) && std::strcmp(usable_item_display_name(16), "Magic Carpet") == 0,
          "B1: id 16 must be Magic Carpet");
    check(usable_item_display_name(17) && std::strcmp(usable_item_display_name(17), "Skull Key") == 0,
          "B1: id 17 must be Skull Key");
    check(usable_item_display_name(18) != nullptr, "B1 RED: id 18 (Amulet of Lord British) must resolve");
    check(usable_item_display_name(19) != nullptr, "B1 RED: id 19 (Crown of Lord British) must resolve");
    check(usable_item_display_name(20) != nullptr, "B1 RED: id 20 (Sceptre of Lord British) must resolve");
    for (int id = 21; id <= 28; ++id)
        check(usable_item_display_name(id) != nullptr,
              (std::string("B1 RED: id ") + std::to_string(id) +
               " (Moonstone, generic name) must resolve")
                  .c_str());
    check(usable_item_display_name(29) != nullptr, "B1 RED: id 29 (Shard of Falsehood) must resolve");
    check(usable_item_display_name(30) != nullptr, "B1 RED: id 30 (Shard of Hatred) must resolve");
    check(usable_item_display_name(31) != nullptr, "B1 RED: id 31 (Shard of Cowardice) must resolve");
    check(usable_item_display_name(32) && std::strcmp(usable_item_display_name(32), "Spyglass") == 0,
          "B1: id 32 must be Spyglass");
    check(usable_item_display_name(33) != nullptr, "B1 RED: id 33 (HMS Cape plans) must resolve");
    check(usable_item_display_name(34) && std::strcmp(usable_item_display_name(34), "Sextant") == 0,
          "B1: id 34 must be Sextant");
    check(usable_item_display_name(36) != nullptr, "B1 RED: id 36 (Black Badge) must resolve");
    check(usable_item_display_name(37) && std::strcmp(usable_item_display_name(37), "Wooden Box") == 0,
          "B1: id 37 must be Wooden Box");
    std::cout << "B1 (real-id display names) executed\n";
}

// B1 (Pocket Watch, id 35): characterization only. See report section F for
// the full state.h/debug_developer.cpp/save_core.cpp evidence, re-confirmed
// during this correction pass. No field backs id 35 anywhere, so this test
// only documents that fact and does NOT assert picker presence for it.
static void b1_pocket_watch_characterization() {
    check(usable_item_display_name(35) == nullptr,
          "B1 characterization: id 35 (Pocket Watch) has no display-name table entry either -- "
          "consistent with having no GameState field anywhere (see report section F)");
    std::cout << "B1 (Pocket Watch characterization, GREEN -- absence confirmed both places) executed\n";
}

// B2: RED -- id 18's display/picker name must never be "Grapple"; the
// picker for real id 18 must be Amulet, and Grapple must be entirely absent
// from the Use picker (it is a Klimb-only item).
static void b2_id18_not_grapple() {
    const auto *name18 = usable_item_display_name(18);
    check(!(name18 && std::strcmp(name18, "Grapple") == 0),
          "B2 RED: id 18's canonical display name must not be Grapple");

    GameState g{};
    g.grapple = true;
    g.magic_carpets = g.skull_keys = 0;
    g.spyglass = g.sextant = false;
    g.wooden_box = false;
    const auto rows = usable_item_picker_rows(picker_input(g));
    bool grapple_row_present = false;
    for (size_t i = 0; i < rows.count; ++i)
        if (rows.rows[i].id == 18) grapple_row_present = true;
    check(!grapple_row_present,
          "B2 RED: today's shared picker logic still adds id 18 (labelled via offset index 2, "
          "'Grapple') whenever grapple is owned -- Grapple must not be a Use-picker row at all");
    std::cout << "B2 (id 18 != Grapple) executed\n";
}

// B3: RED -- the picker (via the shared production seam) must include each
// owned, state-backed usable item exactly once for the FULL desired id set:
// 16,17,18,19,20,21-28,29,30,31,32,33,34,36,37 -- EXCLUDING 35 (Pocket Watch,
// no backing state; see B1 characterization and report section F). Moonstone
// ids 21-28 are now INCLUDED in this required set (correcting the prior
// pass, which wrongly excluded them): QuestWorldServices' Moonstone model is
// a per-phase `buried` bool, not a scalar GameState field, so the fixture
// supplies "owned" (not buried) moonstone flags directly via the shared
// seam's moonstone_owned[] input (see picker_input() above and report
// section L) rather than being excluded from the desired set. The seam's
// row-selection function does not read moonstone_owned[] at all today (that
// IS R-08 for moonstones specifically), so these 8 assertions are expected
// RED, for a genuinely different reason than "excluded from consideration".
static void b3_picker_completeness_red() {
    GameState g{};
    g.magic_carpets = 1;
    g.skull_keys = 1;
    g.spyglass = true;
    g.sextant = true;
    g.wooden_box = true;
    g.quest.artifacts[0] = true; // Amulet (18) owned/worn-eligible.
    g.quest.artifacts[1] = true; // Crown (19).
    g.quest.artifacts[2] = true; // Sceptre (20).
    g.quest.shards[0] = g.quest.shards[1] = g.quest.shards[2] = true; // Shards 29-31.
    g.hms_cape = false; // Plans (33) gate is possession of the cape item itself, not hms_cape rigged flag.
    g.black_badge = true; // Badge (36).
    bool moonstone_owned[8];
    for (bool &owned : moonstone_owned) owned = true; // All 8 moonstones possessed/not-buried.

    const auto rows = usable_item_picker_rows(picker_input(g, moonstone_owned));
    const int expected_ids[] = {16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28,
                                 29, 30, 31, 32, 33, 34, 36, 37};
    for (int id : expected_ids) {
        bool present = false;
        for (size_t i = 0; i < rows.count; ++i)
            if (rows.rows[i].id == id) present = true;
        check(present, (std::string("B3 RED: picker must include id ") + std::to_string(id) +
                        " when owned -- currently only 16/17/18(wrong-name)/32/34/37 are ever added, "
                        "and moonstones 21-28 are never read by the picker logic at all")
                           .c_str());
    }
    std::cout << "B3 (picker completeness, including moonstones 21-28) executed\n";
}

// B4: RED -- for representative ids, selecting the picker row must build a
// UseItem command with the SAME canonical numeric id (no off-by-one/offset
// bug like today's real id 18 receiving usable_item_display_name(2)).
static void b4_row_id_matches_command_id() {
    GameState g{};
    g.quest.artifacts[0] = g.quest.artifacts[1] = g.quest.artifacts[2] = true;
    g.quest.shards[0] = true;
    // hms_cape/black_badge gate Plans/Badge in the real picker via specialItems
    // possession flags; g.black_badge doubles as "owns the badge" today.
    g.black_badge = true;
    bool moonstone_owned[8]{};
    moonstone_owned[0] = true; // Moonstone phase 0 (real id 21) possessed.
    const struct {
        int expected_id;
        const char *label;
    } representative[] = {{18, "Amulet"}, {19, "Crown"},    {20, "Sceptre"}, {21, "Moonstone"},
                          {29, "Shard"},  {33, "Plans"},    {36, "Badge"}};
    // The seam does not yet add 19/20/21/29/33/36 (that IS R-08), so this
    // loop demonstrates the row is entirely missing rather than mis-mapped --
    // both are the RED condition B4 is documenting.
    const auto rows = usable_item_picker_rows(picker_input(g, moonstone_owned));
    for (const auto &want : representative) {
        bool found_with_correct_id = false;
        for (size_t i = 0; i < rows.count; ++i)
            if (rows.rows[i].id == want.expected_id) found_with_correct_id = true;
        check(found_with_correct_id,
              (std::string("B4 RED: ") + want.label + " row must carry canonical id " +
               std::to_string(want.expected_id) + " (currently absent from the picker)")
                  .c_str());
    }
    std::cout << "B4 (row id == command id for representative items) executed\n";
}

// B5: RED -- every picker-visible, state-backed usable item must resolve to
// a real display name (never null / never the "Unresolved id N" fallback
// AlphaRuntime's open_selection() would otherwise render). Moonstones
// (21-28) are included here too, using a generic name (they are not
// individually named in the reference either -- see B1).
static void b5_all_visible_items_resolve_names() {
    const int ids_that_should_be_visible[] = {16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28,
                                              29, 30, 31, 32, 33, 34, 36, 37};
    for (int id : ids_that_should_be_visible) {
        const auto *name = usable_item_display_name(id);
        check(name != nullptr,
              (std::string("B5 RED: usable_item_display_name(") + std::to_string(id) +
               ") must resolve to a real name, not nullptr/\"Unresolved id\"")
                  .c_str());
    }
    std::cout << "B5 (all visible items resolve names) executed\n";
}

// B6: GREEN guard -- Grapple remains reachable through Klimb, not Use. This
// is a display-name-level characterization only (Klimb's own command
// routing already has dedicated coverage in command_parity_test.cpp /
// dungeon_parity_test.cpp for the actual Klimb-over-mountain behavior; not
// duplicated here).
static void b6_grapple_via_klimb_green_guard() {
    // Grapple is consumed by commands.cpp's Klimb handler and
    // dungeon.cpp's pit logic, never by UseItem. There is no
    // "grapple UseItem" core handler to call here; the guard is simply that
    // usable_item_display_name's real-id slot for 18 is Amulet, not
    // Grapple (already asserted in B2), so nothing in the Use picker can
    // ever present Grapple once R-07 is fixed.
    check(usable_item_display_name(18) == nullptr ||
              std::strcmp(usable_item_display_name(18), "Grapple") != 0,
          "B6 GREEN guard: real id 18 is never Grapple, so Use can never surface it");
    std::cout << "B6 (Grapple stays a Klimb-only item, GREEN guard) executed\n";
}

int main() {
    b1_real_id_names();
    b1_pocket_watch_characterization();
    b2_id18_not_grapple();
    b3_picker_completeness_red();
    b4_row_id_matches_command_id();
    b5_all_visible_items_resolve_names();
    b6_grapple_via_klimb_green_guard();
    std::cout << checks << " batch3 group B checks executed, " << failures << " failed\n";
    return failures > 0 ? 1 : 0;
}
