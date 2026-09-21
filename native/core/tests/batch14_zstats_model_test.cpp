// Batch 14 / R-22 -- the (Z)-stats presentation model.
//
// These guards drive openu5::compose_zstats_page()/zstats_list()/the axis
// helpers -- the SAME production code the T-Deck frontend composes its panel
// from (AlphaRuntime::compose_selection_view()).  There is no host-only
// mirror of the formatting here: if a row string changes, the device changes
// with it.
//
// Reference authority for every expectation is cited inline against
// re/notes/zstats.md and re/notes/ztats-layout.md (which are themselves
// instruction-by-instruction derivations of ZSTATS.OVL and the DATA.OVL
// DGROUP).  Nothing asserts a stock-party value: every fixture mutates state
// deliberately and distinctly so field aliasing is visible.
#include "openu5/zstats.h"

#include "openu5/display_names.h"
#include "openu5/inventory.h"

#include <cstdio>
#include <cstring>

using namespace openu5;

namespace {

int g_failures = 0, g_checks = 0;

void expect(bool ok, const char *id, const char *what) {
    ++g_checks;
    std::printf("  %-5s %-6s %s\n", ok ? "GREEN" : "RED", id, what);
    if (!ok) ++g_failures;
}

void expect_row(const ZStatsPage &page, size_t row, const char *want, const char *id,
                const char *what) {
    const bool ok = row < page.row_count && std::strcmp(page.rows[row].text, want) == 0;
    ++g_checks;
    std::printf("  %-5s %-6s %s\n", ok ? "GREEN" : "RED", id, what);
    if (!ok) {
        ++g_failures;
        std::printf("         want [%s]\n         got  [%s] (row_count=%u)\n", want,
                    row < page.row_count ? page.rows[row].text : "<no such row>",
                    unsigned(page.row_count));
    }
}

void name_member(CharacterState &c, const char *name) {
    std::snprintf(c.name, sizeof(c.name), "%s", name);
}

// A three-member party whose members are DELIBERATELY distinct in every
// printed field, so any accidental aliasing between members or between stat
// slots shows up as a wrong string rather than a coincidentally right one.
GameState three_member_party() {
    GameState g{};
    g.party.character_count = 3;
    g.party.party_size = 3;
    g.party.active_character = 0;

    auto &a = g.party.characters[0];
    name_member(a, "Alric");
    a.gender = 0x0b; // male; CP437 0x0b, the same constant shops.cpp reads
    a.character_class = 'A';
    a.status = 'G';
    a.strength = 21; a.dexterity = 22; a.intelligence = 23;
    a.current_hp = 111; a.max_hp = 222; a.exp = 333;
    a.level = 4; a.current_mp = 17;
    a.helmet = 2;   // Iron Helm
    a.armor = 14;   // Plate Mail
    a.weapon = 30;  // Long Sword
    a.shield = 5;   // Large Shield
    a.ring = 43;    // Ring of Protection
    a.amulet = 45;  // Amulet/Turning

    auto &b = g.party.characters[1];
    name_member(b, "Brianne");
    b.gender = 0x0c; // female
    b.character_class = 'M';
    b.status = 'P'; // Poisoned
    b.strength = 8; b.dexterity = 9; b.intelligence = 31;
    b.current_hp = 12; b.max_hp = 48; b.exp = 1200;
    b.level = 6; b.current_mp = 31;
    b.helmet = b.armor = b.weapon = b.shield = b.ring = b.amulet = kEquipmentNothing;

    auto &c = g.party.characters[2];
    name_member(c, "Cordell");
    c.gender = 0x0b;
    c.character_class = 'S'; // Shepherd
    c.status = 'D';          // Dead
    c.strength = 5; c.dexterity = 6; c.intelligence = 7;
    c.current_hp = 0; c.max_hp = 30; c.exp = 8;
    c.level = 1; c.current_mp = 0;
    c.helmet = c.armor = c.shield = c.ring = c.amulet = kEquipmentNothing;
    c.weapon = 18; // Club -- one equipped slot only

    g.food = 1234;
    g.gold = 567;
    g.keys = 8;
    g.gems = 9;
    g.torches = 10;
    g.grapple = true;
    return g;
}

ZStatsInput input_for(const GameState &g, uint8_t moonstones = 0) {
    ZStatsInput in{};
    in.game = &g;
    in.moonstones_owned = moonstones;
    return in;
}

// --- Z8 / axis ------------------------------------------------------------
void test_axis() {
    std::printf("Z8. Page axis -- the 17-slot ring of ztats-layout.md section 2\n");
    expect(zstats_page_kind(0) == ZStatsPageKind::Stats &&
               zstats_page_kind(10) == ZStatsPageKind::Stats,
           "Z8-1", "even per-member slots are Stats pages (draw_stat_page 0x0082)");
    expect(zstats_page_kind(1) == ZStatsPageKind::Arms &&
               zstats_page_kind(11) == ZStatsPageKind::Arms,
           "Z8-2", "odd per-member slots are Arms pages (draw_arms_page 0x02a8)");
    expect(zstats_page_kind(kZStatsPageProvisions) == ZStatsPageKind::Provisions,
           "Z8-3", "slot 0xc is Provisions (draw_provisions 0x039c)");
    expect(zstats_page_kind(kZStatsPageReagents) == ZStatsPageKind::List &&
               zstats_page_kind(kZStatsPageSpells) == ZStatsPageKind::List &&
               zstats_page_kind(kZStatsPageItems) == ZStatsPageKind::List &&
               zstats_page_kind(kZStatsPageArmaments) == ZStatsPageKind::List,
           "Z8-4", "slots 0xd-0x10 are the four scroll lists (render_item_list 0x06e8)");
    expect(zstats_member_of_page(0) == 0 && zstats_member_of_page(1) == 0 &&
               zstats_member_of_page(6) == 3 && zstats_member_of_page(7) == 3 &&
               zstats_member_of_page(kZStatsPageItems) == -1,
           "Z8-5", "member = page>>1 on per-member slots, none on the global pages");

    // Forward ring for a 3-member party: 0..5, then the wrap gate at party*2-1.
    int page = 0;
    const int want[] = {1, 2, 3, 4, 5, 0xc, 0xd, 0xe, 0xf, 0x10, 0};
    bool forward_ok = true;
    for (int step : want) {
        page = zstats_axis_next(page, 3);
        if (page != step) forward_ok = false;
    }
    expect(forward_ok, "Z8-6",
           "next() walks 0->5, then jumps the dead window to 0xc and wraps 0x10->0");

    // Reverse ring: symmetric, per the 0xaa6/0xac6 gates.
    page = 0;
    const int back[] = {0x10, 0xf, 0xe, 0xd, 0xc, 5, 4, 3, 2, 1, 0};
    bool reverse_ok = true;
    for (int step : back) {
        page = zstats_axis_prev(page, 3);
        if (page != step) reverse_ok = false;
    }
    expect(reverse_ok, "Z8-7",
           "prev() wraps 0->0x10 and steps 0xc back to the last member's Arms page");

    bool never_dead = true;
    for (int party = 1; party <= 6; ++party) {
        int p = 0;
        for (int i = 0; i < 64; ++i) {
            p = zstats_axis_next(p, party);
            if (p >= party * 2 && p < kZStatsPageProvisions) never_dead = false;
        }
        p = 0;
        for (int i = 0; i < 64; ++i) {
            p = zstats_axis_prev(p, party);
            if (p >= party * 2 && p < kZStatsPageProvisions) never_dead = false;
        }
    }
    expect(never_dead, "Z8-8",
           "for every party size 1-6 the ring never enters the dead window [party*2,0xb]");
}

// --- Z13 / party-size edge cases -----------------------------------------
void test_party_edges() {
    std::printf("Z13. Party-size edge cases\n");
    expect(zstats_axis_next(1, 1) == kZStatsPageProvisions, "Z13-1",
           "a solo party steps from its Arms page straight to Provisions (party*2-1 == 1)");
    expect(zstats_axis_prev(kZStatsPageProvisions, 1) == 1, "Z13-2",
           "and back into that same Arms page");
    const int clamped = zstats_clamp_page(9, 2);
    expect(clamped < 4 || clamped >= kZStatsPageProvisions, "Z13-3",
           "a page stranded in the dead window by a shrinking roster is clamped out of it");
    expect(zstats_clamp_page(kZStatsPageArmaments, 1) == kZStatsPageArmaments, "Z13-4",
           "a global page is never clamped away by party size");
    expect(zstats_clamp_page(0, 0) == 0, "Z13-5", "an empty roster is handled without stranding");

    GameState g = three_member_party();
    g.party.party_size = 1;
    g.party.character_count = 1;
    const auto page = compose_zstats_page(input_for(g), 0, 0);
    expect(page.member == 0 && page.row_count > 0, "Z13-6",
           "the surviving member's stats page still composes after the roster shrinks");
}

// --- Z2 / live character data --------------------------------------------
void test_stats_page() {
    std::printf("Z2. Stats page -- live, per-member, distinct fields\n");
    GameState g = three_member_party();

    // Row order and the two-column interleave are draw_stat_page 0x0082
    // (ztats-layout.md 8.1): the LEVEL is on the header line, HM is maxHP and
    // Ex is experience.
    auto a = compose_zstats_page(input_for(g), 0, 0);
    expect(a.kind == ZStatsPageKind::Stats && a.member == 0, "Z2-1",
           "page 0 is member 0's stats page");
    expect(std::strcmp(a.banner, "Alric") == 0, "Z2-2",
           "the banner is the member's name (0x6c70 prints the record's name field)");
    expect_row(a, 0, " M Lv-4 Avatar", "Z2-3",
               "header line: sex, ' Lv-' + level (record +0x16), class from AMBFDTPRS");
    expect_row(a, 1, "  Good Health", "Z2-4",
               "health line, centred (control 0xfc), from the 0x1a6a letter table");
    // Column spacing note.  The two spaces before each right-hand label are
    // part of the DGROUP strings themselves ("  HP:" = DS 0x96e2, "  HM:" =
    // 0x96ee, "  Ex:" = 0x96fa), and the value that follows is a bare
    // print_number field -- 4 wide, space padded -- with no extra gap.  The
    // schematic "Str=NN  HP:  NNNN" in ztats-layout.md section 8.1 shows the
    // field, not literal cells.
    expect_row(a, 3, "Str=21  HP: 111", "Z2-5",
               "Str (+0x0c, 2 digits) and current HP (+0x10, 4 digits) share row 3");
    expect_row(a, 4, "Int=23  HM: 222", "Z2-6",
               "Int (+0x0e) and HM = MAX HP (+0x12) share row 4");
    expect_row(a, 5, "Dex=22  Ex: 333", "Z2-7",
               "Dex (+0x0d) and Ex = experience (+0x14) share row 5");
    expect_row(a, 7, "    Magic:17", "Z2-8",
               "Magic = current MP (+0x0f), indented 4, space-padded");

    // Every printed field of member 1 differs from member 0's.
    auto b = compose_zstats_page(input_for(g), 2, 0);
    expect(std::strcmp(b.banner, "Brianne") == 0 && b.member == 1, "Z2-9",
           "page 2 is member 1 -- the name follows the page, not the active character");
    expect_row(b, 0, " F Lv-6 Mage", "Z2-10", "member 1 renders her own sex, level and class");
    expect_row(b, 3, "Str=08  HP:  12", "Z2-11",
               "attributes zero-pad to 2 and HP space-pads to 4 (print_number's pad byte)");
    expect_row(b, 5, "Dex=09  Ex:1200", "Z2-12", "a four-digit experience fills the column");

    std::printf("Z12. Abnormal status lines\n");
    // Centring width.  draw_stat_page's second set_text_window call is
    // (1, 0x18, 1, 0x27, 9) -- cols 24..39, SIXTEEN cells -- and the kernel's
    // 0xfc control centres inside the active window, so that is the width the
    // health line and the "Arms"/"(None ...)" lines centre in.  (The faithful
    // TypeScript skin narrows its own panel to 15 for roster alignment; that
    // is its refinement, not the asm's.)
    expect_row(b, 1, "    Poisoned", "Z12-1", "status P renders Poisoned");
    auto c = compose_zstats_page(input_for(g), 4, 0);
    expect_row(c, 1, "      Dead", "Z12-2", "status D renders Dead");
    g.party.characters[2].status = 'S';
    c = compose_zstats_page(input_for(g), 4, 0);
    expect_row(c, 1, "     Asleep", "Z12-3", "status S renders Asleep");
    g.party.characters[2].status = 'C';
    c = compose_zstats_page(input_for(g), 4, 0);
    expect_row(c, 1, "    Charmed", "Z12-4", "status C renders Charmed");

    // Mutation must propagate -- no snapshot, no stale copy.
    g.party.characters[0].strength = 99;
    g.party.characters[0].current_hp = 7;
    g.party.characters[0].level = 9;
    a = compose_zstats_page(input_for(g), 0, 0);
    expect_row(a, 3, "Str=99  HP:   7", "Z2-13",
               "mutating Str/HP is reflected on the next compose");
    expect_row(a, 0, " M Lv-9 Avatar", "Z2-14", "mutating the level is reflected too");
}

// --- Z3 / equipment naming -----------------------------------------------
void test_arms_page() {
    std::printf("Z3. Arms page -- canonical equipment names, no numeric placeholders\n");
    GameState g = three_member_party();

    auto a = compose_zstats_page(input_for(g), 1, 0);
    expect(a.kind == ZStatsPageKind::Arms && a.member == 0, "Z3-1",
           "page 1 is member 0's Arms page");
    expect(std::strcmp(a.banner, "Alric") == 0, "Z3-2",
           "the Arms banner is still the member's name");
    expect_row(a, 0, "      Arms", "Z3-3", "'Arms' title, centred (0x970e, control 0xfc)");
    // Slot order is the record's own: +0x19 helm, +0x1a armour, +0x1b hand A,
    // +0x1c hand B, +0x1d ring, +0x1e amulet (zstats.md, record layout).
    expect_row(a, 2, " Iron Helm", "Z3-4", "helmet slot first, by canonical name");
    expect_row(a, 3, " Plate Mail", "Z3-5", "armour slot second");
    expect_row(a, 4, " Long Sword", "Z3-6",
               "hand A: equip id 30 is the Long Sword of the TYPE table's id space");
    expect_row(a, 5, " Large Shield", "Z3-7", "hand B fourth");
    expect_row(a, 6, " Ring of Protection", "Z3-8", "ring slot fifth");
    expect_row(a, 7, " Amulet/Turning", "Z3-9", "amulet slot last");

    auto b = compose_zstats_page(input_for(g), 3, 0);
    expect_row(b, 4, "  (None ready)", "Z3-10",
               "a member with every slot empty gets 0x9716 (None ready), not blank rows");

    auto c = compose_zstats_page(input_for(g), 5, 0);
    expect_row(c, 2, " Club", "Z3-11",
               "empty slots are SKIPPED (print_padded_string returns 0 on 0xff), so the one "
               "equipped item is the first content row");
    expect(c.row_count == 3, "Z3-12", "and no empty slot leaves a stray row behind");

    // is_generic_identifier_label is the project's own placeholder detector.
    bool clean = true;
    for (int id = 0; id < 48; ++id) {
        const char *n = equipment_display_name(id);
        if (!n || !*n || is_generic_identifier_label(n)) clean = false;
    }
    expect(clean, "Z3-13",
           "all 48 equipment ids name a real item -- no 'Equipment <id>' placeholder");

    // The off-by-one guard.  These anchors are pinned by the binary itself,
    // independently of any name table: equip_type_of() is a verbatim copy of
    // DATA.OVL DS 0x1a7e, and ammo_item_for()/is_thrown_weapon() cite
    // ZSTATS.OVL:0x0d0c and COMSUBS:0x097c.
    expect(std::strcmp(equipment_display_name(16), "Dagger") == 0, "Z3-14",
           "id 16 is the Dagger -- COMSUBS 0x097c's thrown set is {0x10,0x15,0x16}");
    expect(std::strcmp(equipment_display_name(27), "Arrows") == 0 &&
               std::strcmp(equipment_display_name(29), "Quarrels") == 0,
           "Z3-15", "ids 27/29 are the ammunition ZSTATS 0x0c82 refuses to equip");
    expect(std::strcmp(equipment_display_name(42), "Ring of Invisibility") == 0, "Z3-16",
           "id 42 is the Ring of Invisibility (ZSTATS 0x0cbf / 0x0e01)");
    bool armour_band = true;
    for (int id = 9; id <= 15; ++id)
        if (slot_for_equip(id) != EquipSlot::Armor) armour_band = false;
    expect(armour_band, "Z3-17",
           "ids 9-15 are the armour band ZSTATS 0x0c94 locks in battle -- the name table "
           "must share that id space");
    expect(std::strcmp(equipment_display_name(9), "Cloth Armour") == 0, "Z3-18",
           "and id 9, the first of that band, names an armour rather than a shield");
    expect(equipment_display_name(47) && std::strcmp(equipment_display_name(47), "Ankh") == 0,
           "Z3-19", "the table runs to id 47 (Ankh), the last entry of DS 0x17f6");

    // No page may print a fabricated "<Category> <id>" label.  Sweep every
    // page of a fully-stocked party through the project's own detector.
    GameState stocked = three_member_party();
    for (int i = 0; i < 48; ++i) stocked.equipment_quantities[i] = 1;
    for (int i = 0; i < 48; ++i) stocked.spell_quantities[i] = 1;
    for (int i = 0; i < 8; ++i) {
        stocked.reagent_quantities[i] = 1;
        stocked.scroll_quantities[i] = 1;
        stocked.potion_quantities[i] = 1;
    }
    stocked.magic_carpets = stocked.skull_keys = 1;
    stocked.spyglass = stocked.sextant = stocked.wooden_box = true;
    stocked.hms_cape = stocked.black_badge = true;
    for (int i = 0; i < 3; ++i) stocked.quest.artifacts[i] = stocked.quest.shards[i] = true;
    bool no_placeholder = true;
    for (int page = 0; page <= kZStatsPageArmaments; ++page)
        for (size_t scroll = 0; scroll < 48; scroll += 7) {
            const auto composed = compose_zstats_page(input_for(stocked, 0xff), page, scroll);
            for (size_t row = 0; row < composed.row_count; ++row) {
                const char *text = composed.rows[row].text;
                // Rows carry a quantity prefix, so test the name part too.
                const char *dash = std::strchr(text, '-');
                if (is_generic_identifier_label(text)) no_placeholder = false;
                if (dash && is_generic_identifier_label(dash + 1)) no_placeholder = false;
            }
        }
    expect(no_placeholder, "Z3-20",
           "no page of a fully-stocked party prints a generic '<Category> <id>' label");
}

// --- Provisions -----------------------------------------------------------
void test_provisions_page() {
    std::printf("Z4a. Provisions page (draw_provisions 0x039c)\n");
    GameState g = three_member_party();
    auto p = compose_zstats_page(input_for(g), kZStatsPageProvisions, 0);
    expect(p.kind == ZStatsPageKind::Provisions && p.member == -1, "Z4a-1",
           "Provisions is a global page with no member");
    expect(std::strcmp(p.banner, "Equipment") == 0, "Z4a-2",
           "its banner is 0x9724 Equipment, not 'Provisions'");
    expect_row(p, 1, " Food: 1234", "Z4a-3", "Food, 4 digits, space padded (0x972e)");
    expect_row(p, 2, " Gold:  567", "Z4a-4", "Gold, same column (0x9738)");
    expect_row(p, 4, " Keys....... 8", "Z4a-5", "Keys with the 7-dot leader of 0x9742");
    expect_row(p, 5, " Gems....... 9", "Z4a-6", "Gems (0x9752)");
    expect_row(p, 6, " Torches....10", "Z4a-7", "Torches with the 4-dot leader of 0x9760");
    expect_row(p, 7, " Grapple", "Z4a-8", "the Grapple line appears only when owned (0x976e)");

    g.grapple = false;
    p = compose_zstats_page(input_for(g), kZStatsPageProvisions, 0);
    expect(p.row_count == 7, "Z4a-9", "and disappears entirely when it is not");

    g.food = 0; g.gold = 0; g.keys = 0; g.gems = 0; g.torches = 0;
    p = compose_zstats_page(input_for(g), kZStatsPageProvisions, 0);
    expect_row(p, 1, " Food:    0", "Z4a-10",
               "Provisions counters are NOT hidden at zero -- the original prints the number");
    expect_row(p, 4, " Keys....... 0", "Z4a-11", "same for the dotted counters");
}

// --- Z4 / Z5 / Z6 : the four lists ---------------------------------------
void test_lists() {
    std::printf("Z4/Z5/Z6. The four scroll lists (render_item_list 0x06e8)\n");
    GameState g = three_member_party();

    auto r = compose_zstats_page(input_for(g), kZStatsPageReagents, 0);
    expect(std::strcmp(r.banner, "Reagents") == 0, "Z5-1", "list titles: 0x97ac Reagents");
    expect(r.list_total == 0, "Z6-1", "with no reagents owned the list is empty");
    expect_row(r, 0, " (None owned!)", "Z6-2",
               "an empty list prints 0x9794 (None owned!)");

    g.reagent_quantities[0] = 3;  // Sulfur Ash
    g.reagent_quantities[1] = 0;  // Ginseng -- owned none
    g.reagent_quantities[5] = 12; // Black Pearl
    r = compose_zstats_page(input_for(g), kZStatsPageReagents, 0);
    expect(r.list_total == 2, "Z6-3",
           "only qty>0 rows are listed (find_next_owned 0x05a4 skips zeros)");
    expect_row(r, 0, " 3-Sulfur Ash", "Z4-1",
               "row format: 2-digit space-padded quantity, '-' separator (0x2d), name");
    expect_row(r, 1, "12-Black Pearl", "Z5-2",
               "list order is the table's own index order, so Black Pearl follows Sulfur Ash");
    expect(std::strcmp(r.rows[0].text, " 0-Ginseng") != 0, "Z6-4",
           "a zero-count reagent is absent, not rendered with a zero");

    g.reagent_quantities[0] = 99;
    r = compose_zstats_page(input_for(g), kZStatsPageReagents, 0);
    expect_row(r, 0, "99-Sulfur Ash", "Z4-2", "a mutated count re-renders");

    // Armaments -- the equipment pack, the same table the Ready picker reads.
    g.equipment_quantities[16] = 2;  // Dagger
    g.equipment_quantities[30] = 1;  // Long Sword
    g.equipment_quantities[27] = 24; // Arrows
    auto arm = compose_zstats_page(input_for(g), kZStatsPageArmaments, 0);
    expect(std::strcmp(arm.banner, "Armaments") == 0, "Z5-3", "0x97c4 Armaments");
    expect(arm.list_total == 3, "Z4-3", "three owned equipment lines");
    expect_row(arm, 0, " 2-Dagger", "Z5-4", "index order: 16 Dagger");
    expect_row(arm, 1, "24-Arrows", "Z5-5", "then 27 Arrows");
    expect_row(arm, 2, " 1-Long Sword", "Z5-6", "then 30 Long Sword");

    // Equipped-vs-owned: the pack count is independent of what is worn.
    expect(is_item_equipped(g.party.characters[0], 30) && g.equipment_quantities[30] == 1,
           "Z4-4", "a worn item and a packed copy coexist; the list reports the PACK count");

    // Spells -- 0x57f0, the mixed-spell quantities.
    g.spell_quantities[4] = 5;  // Mani
    g.spell_quantities[47] = 1; // An Tym
    auto sp = compose_zstats_page(input_for(g), kZStatsPageSpells, 0);
    expect(std::strcmp(sp.banner, "Spells") == 0, "Z5-7",
           "0x97b6 is Spells -- list 0xe is the spell mixtures, not a second item page");
    expect_row(sp, 0, " 5-Mani", "Z4-5", "mixed spell counts render like any other list row");
    expect_row(sp, 1, " 1-An Tym", "Z5-8", "and keep the spell table's own order");

    // Items -- the extended quest table 0xB9EE.
    g.scroll_quantities[2] = 1; // In Sanct
    g.potion_quantities[0] = 4; // Blue
    g.magic_carpets = 2;
    g.skull_keys = 7;
    g.quest.artifacts[1] = true; // Crown
    g.wooden_box = true;
    auto it = compose_zstats_page(input_for(g, /*moonstones=*/0x01), kZStatsPageItems, 0);
    expect(std::strcmp(it.banner, "Items") == 0, "Z5-9", "0x97be Items");
    expect_row(it, 0, " 1-In Sanct Scroll", "Z5-10",
               "extended-table order starts with the scrolls (0x5820)");
    expect_row(it, 1, " 4-Blue Potion", "Z5-11", "then the potions (0x5828)");
    expect_row(it, 2, " 2-Magic Carpet", "Z5-12", "then the countable carpets");
    expect_row(it, 3, " 7-Skull Key", "Z5-13", "then the countable skull keys");
    expect_row(it, 4, "Crown of Lord British", "Z6-5",
               "regalia are stored 0xff, so print_list_row 0x062e drops BOTH the number "
               "and the separator");
    expect_row(it, 5, "Moonstone", "Z5-14", "carried moonstones follow the regalia");
    expect_row(it, 6, "Wooden Box", "Z5-15", "and the quest tools come last");

    g.magic_carpets = 0;
    it = compose_zstats_page(input_for(g, 0x01), kZStatsPageItems, 0);
    expect_row(it, 2, " 7-Skull Key", "Z5-16",
               "dropping a countable closes the gap without reordering the survivors");
}

// --- list paging ----------------------------------------------------------
void test_list_paging() {
    std::printf("Z8b. List paging -- 7 content rows (render_item_list 0x746)\n");
    GameState g = three_member_party();
    for (int i = 0; i < 12; ++i) g.equipment_quantities[i] = i + 1;

    auto page = compose_zstats_page(input_for(g), kZStatsPageArmaments, 0);
    expect(page.list_total == 12, "Z8b-1", "twelve owned armaments");
    expect(page.row_count == kZStatsListRows, "Z8b-2",
           "a full list page shows exactly seven rows, the frame's content height");
    expect(!page.more_above && page.more_below, "Z8b-3",
           "at scroll 0 there is more below and nothing above");
    expect(zstats_max_scroll(12) == 5, "Z8b-4", "max scroll is total-7");

    page = compose_zstats_page(input_for(g), kZStatsPageArmaments, 5);
    expect(page.row_count == kZStatsListRows && page.more_above && !page.more_below, "Z8b-5",
           "at max scroll the window is still full and the markers flip");
    expect_row(page, 6, "12-Ring Mail", "Z8b-6", "and the last owned entry is the last row");

    page = compose_zstats_page(input_for(g), kZStatsPageArmaments, 99);
    expect(page.list_scroll == 5, "Z8b-7",
           "an over-large scroll is clamped, never read past the end");
    expect(zstats_max_scroll(3) == 0, "Z8b-8",
           "a list that fits has no scroll range at all");
}

} // namespace

int main() {
    std::printf("Batch 14 / R-22 -- Z-stats presentation model\n\n");
    test_axis();
    test_party_edges();
    test_stats_page();
    test_arms_page();
    test_provisions_page();
    test_lists();
    test_list_paging();
    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
