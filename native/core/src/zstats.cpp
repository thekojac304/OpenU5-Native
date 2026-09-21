// R-22 -- the (Z)-stats page family, composed from live GameState.
//
// Derived instruction by instruction from ZSTATS.OVL and the DATA.OVL DGROUP
// tables it reads; see re/notes/zstats.md (function map, record layout) and
// re/notes/ztats-layout.md (page axis, per-page field order, list renderer).
// Offsets in the comments below are FILE offsets into ZSTATS.OVL, or DS
// offsets into the shared DGROUP, exactly as those notes cite them.
//
// Presentation only.  Nothing here writes GameState, reads a clock or draws
// an RNG: cmd_zstats (0x0a3a) consumes exactly zero rolls and charges no turn.
#include "openu5/zstats.h"

#include "openu5/display_names.h"
#include "openu5/inventory.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace openu5 {
namespace {

// The original's panel is 16 cells wide; every centred line below centres
// inside that same width so the columns line up with the reference's.
constexpr int kPanelCells = 16;

void put(ZStatsRow &row, const char *text) {
    std::snprintf(row.text, sizeof(row.text), "%.*s", int(sizeof(row.text) - 1), text);
}

void put_centred(ZStatsRow &row, const char *text) {
    const int length = int(std::strlen(text));
    const int indent = std::max(0, (kPanelCells - length) / 2);
    std::snprintf(row.text, sizeof(row.text), "%*s%.*s", indent, "",
                  int(sizeof(row.text) - 1 - size_t(indent)), text);
}

// print_number(value, digits, pad) -- the kernel's 0x1a3e, reached through
// ZSTATS' 0x385e thunk.  Attributes use pad '0' (0x30); the word fields and
// the provisions counters use pad ' ' (0x20).
void format_number(char *out, size_t capacity, int32_t value, int digits, char pad) {
    if (pad == '0') std::snprintf(out, capacity, "%0*ld", digits, long(value));
    else std::snprintf(out, capacity, "%*ld", digits, long(value));
}

// draw_stat_page reads the class letter from record +0x0a and indexes the
// pointer table DS 0x1a44.  "AMBFDTPRS" (DS 0x9812) is the letter set --
// nine classes, not four.
const char *class_name_or_null(char code) {
    switch (code) {
    case 'A': return "Avatar";
    case 'M': return "Mage";
    case 'B': return "Bard";
    case 'F': return "Fighter";
    case 'D': return "Druid";
    case 'T': return "Tinker";
    case 'P': return "Paladin";
    case 'R': return "Ranger";
    case 'S': return "Shepherd";
    default: return nullptr;
    }
}

// Health letter from record +0x0b, pointer table DS 0x1a6a.
const char *status_name_or_null(char code) {
    switch (code) {
    case 'G': return "Good Health";
    case 'P': return "Poisoned";
    case 'D': return "Dead";
    case 'S': return "Asleep";
    case 'C': return "Charmed";
    default: return nullptr;
    }
}

int party_bound(const GameState &g) {
    const int size = int(g.party.party_size);
    const int roster = int(g.party.character_count);
    return std::max(0, std::min({size, roster, int(kMaxParty)}));
}

bool member_valid(const GameState &g, int member) {
    return member >= 0 && member < party_bound(g);
}

// One list row.  print_list_row (0x05e2): a 2-digit space-padded quantity,
// the 0x2d separator, then the name -- unless the stored quantity is the 0xff
// sentinel (0x062e), which skips BOTH the number and the separator.
void format_list_row(ZStatsRow &row, const ZStatsListEntry &entry) {
    const char *name = entry.name ? entry.name : "";
    if (entry.quantity_hidden) {
        put(row, name);
        return;
    }
    char quantity[8]{};
    format_number(quantity, sizeof(quantity), std::min<int32_t>(entry.quantity, 99), 2, ' ');
    std::snprintf(row.text, sizeof(row.text), "%s-%s", quantity, name);
}

void push(ZStatsList &list, int32_t index, const char *name, int32_t quantity,
          bool quantity_hidden = false) {
    if (list.count >= kZStatsMaxListEntries || !name || !*name) return;
    auto &entry = list.entries[list.count++];
    entry.index = index;
    entry.name = name;
    entry.quantity = quantity;
    entry.quantity_hidden = quantity_hidden;
}

// List 0xf, the extended item table build_extended_item_table flattens into
// 0xB9EE (ZSTATS 0x099a).  Order is the table's own: scrolls 0x5820, potions
// 0x5828, then the flag/counter items in extended-id order.  The countable
// entries are the scrolls, the potions, the magic carpets (inc/dec at
// CMDS 0x0910 / CAST 0x18a1) and the skull keys (dec at CAST 0x18c4);
// everything else is stored 0xff by its writer and therefore prints with no
// number at all -- the writer census is recorded in
// game/src/skin/coreview.ts::buildQuestList.
ZStatsList build_items_list(const ZStatsInput &in) {
    ZStatsList list{};
    list.title = "Items"; // DS 0x97be
    const auto &g = *in.game;
    for (int i = 0; i < 8; ++i)
        if (g.scroll_quantities[i] > 0) push(list, i, scroll_display_name(i), g.scroll_quantities[i]);
    for (int i = 0; i < 8; ++i)
        if (g.potion_quantities[i] > 0)
            push(list, 8 + i, potion_display_name(i), g.potion_quantities[i]);

    // The flag/counter tail is exactly the (U)se picker's row set, in exactly
    // the same extended-id order -- the two UIs read one table in the
    // original, so they read one seam here (inventory_picker.h).  Reusing it
    // keeps the moonstone gate (owned == not buried) in a single place.
    UsableItemPickerInput picker{};
    picker.magic_carpets = g.magic_carpets;
    picker.skull_keys = g.skull_keys;
    picker.grapple = g.grapple;
    picker.spyglass = g.spyglass;
    picker.sextant = g.sextant;
    picker.wooden_box = g.wooden_box;
    picker.hms_cape = g.hms_cape;
    picker.black_badge = g.black_badge;
    for (int i = 0; i < 3; ++i) picker.artifacts[i] = g.quest.artifacts[i];
    for (int i = 0; i < 3; ++i) picker.shards[i] = g.quest.shards[i];
    for (int i = 0; i < 8; ++i)
        picker.moonstone_owned[i] = (in.moonstones_owned & (1u << unsigned(i))) != 0;

    const auto rows = usable_item_picker_rows(picker);
    for (size_t i = 0; i < rows.count; ++i) {
        const auto &row = rows.rows[i];
        // 0x10 Magic Carpet and 0x11 Skull Key are the only genuinely
        // countable entries of the tail; the rest are stored 0xff.
        const bool countable = row.id == 0x10 || row.id == 0x11;
        push(list, row.id, row.name, countable ? row.quantity : 0, !countable);
    }
    return list;
}

// A quantity table filtered to owned rows, the way find_next_owned (0x05a4)
// walks one: the index order is the table's own, and qty == 0 is skipped.
template <typename Naming>
ZStatsList build_quantity_list(const char *title, const int32_t *quantities, size_t count,
                               Naming naming) {
    ZStatsList list{};
    list.title = title;
    for (size_t i = 0; i < count; ++i)
        if (quantities[i] > 0) push(list, int32_t(i), naming(int32_t(i)), quantities[i]);
    return list;
}

ZStatsPage compose_stats(const ZStatsInput &in, int page) {
    ZStatsPage out{};
    out.kind = ZStatsPageKind::Stats;
    out.page = page;
    const auto &g = *in.game;
    const int member = zstats_member_of_page(page);
    if (!member_valid(g, member)) return out;
    out.member = member;
    const auto &c = g.party.characters[member];
    std::snprintf(out.banner, sizeof(out.banner), "%.*s", int(sizeof(out.banner) - 1), c.name);

    // Row 0: sex glyph, " Lv-" (DS 0x96d6) + level (record +0x16, 1 digit),
    // then the class name.  The original prints the CP437 male/female glyphs
    // 0x0b/0x0c; the T-Deck's 5x7 face is ASCII-only, so the two letters
    // stand in -- a glyph substitution, not a content change.
    const char sex = c.gender == 0x0c ? 'F' : 'M';
    const char *cls = class_name_or_null(c.character_class);
    char header[kZStatsRowChars]{};
    if (cls) std::snprintf(header, sizeof(header), " %c Lv-%u %s", sex, unsigned(c.level), cls);
    else std::snprintf(header, sizeof(header), " %c Lv-%u", sex, unsigned(c.level));
    put(out.rows[0], header);

    // Row 1: the health line, centred (control 0xfc).
    const char *status = status_name_or_null(c.status);
    put_centred(out.rows[1], status ? status : "");

    // Rows 3-5: the two columns of draw_stat_page.  HM is MAX HP (+0x12) and
    // Ex is experience (+0x14) -- not "magic points" and not the level.
    char attribute[8]{}, word[8]{};
    format_number(attribute, sizeof(attribute), c.strength, 2, '0');
    format_number(word, sizeof(word), c.current_hp, 4, ' ');
    std::snprintf(out.rows[3].text, sizeof(out.rows[3].text), "Str=%s  HP:%s", attribute, word);
    format_number(attribute, sizeof(attribute), c.intelligence, 2, '0');
    format_number(word, sizeof(word), c.max_hp, 4, ' ');
    std::snprintf(out.rows[4].text, sizeof(out.rows[4].text), "Int=%s  HM:%s", attribute, word);
    format_number(attribute, sizeof(attribute), c.dexterity, 2, '0');
    format_number(word, sizeof(word), c.exp, 4, ' ');
    std::snprintf(out.rows[5].text, sizeof(out.rows[5].text), "Dex=%s  Ex:%s", attribute, word);

    // Row 7: "    Magic:" (DS 0x9700) + current MP (+0x0f), 2 digits, SPACE
    // padded -- the one attribute-width field that does not pad with '0'.
    format_number(attribute, sizeof(attribute), c.current_mp, 2, ' ');
    std::snprintf(out.rows[7].text, sizeof(out.rows[7].text), "    Magic:%s", attribute);

    out.row_count = 8;
    return out;
}

ZStatsPage compose_arms(const ZStatsInput &in, int page) {
    ZStatsPage out{};
    out.kind = ZStatsPageKind::Arms;
    out.page = page;
    const auto &g = *in.game;
    const int member = zstats_member_of_page(page);
    if (!member_valid(g, member)) return out;
    out.member = member;
    const auto &c = g.party.characters[member];
    std::snprintf(out.banner, sizeof(out.banner), "%.*s", int(sizeof(out.banner) - 1), c.name);

    put_centred(out.rows[0], "Arms"); // DS 0x970e, centred and underlined
    out.row_count = 1;

    // Slot order is the record's own: +0x19 helmet, +0x1a armour, +0x1b hand
    // A, +0x1c hand B, +0x1d ring, +0x1e amulet.  print_padded_string (0x0278)
    // returns 0 for a 0xff slot, so empty slots are SKIPPED rather than
    // leaving a blank line.
    const uint8_t slots[] = {c.helmet, c.armor, c.weapon, c.shield, c.ring, c.amulet};
    size_t row = 2;
    size_t worn = 0;
    for (uint8_t id : slots) {
        if (id == kEquipmentNothing) continue;
        ++worn;
        if (row >= kZStatsPageRows) continue;
        const char *name = equipment_display_name(id);
        char line[kZStatsRowChars]{};
        // All 48 ids of the equipment table resolve, so this fallback can only
        // be reached by a corrupt/out-of-domain slot byte.  It is deliberately
        // a DIAGNOSTIC, not a fabricated item name: the project's own
        // placeholder detector (is_generic_identifier_label) treats
        // "Equipment 12"-style text as a defect, and AlphaRuntime's picker
        // uses this same "unresolved id" wording for the same reason.
        if (name) std::snprintf(line, sizeof(line), " %s", name);
        else std::snprintf(line, sizeof(line), " (unresolved id %u)", unsigned(id));
        put(out.rows[row], line);
        out.row_count = ++row;
    }
    // "(None ready)" (DS 0x9716) on row 4, the asm's own set_cursor(0,4).
    if (!worn) {
        put_centred(out.rows[4], "(None ready)");
        out.row_count = 5;
    }
    return out;
}

ZStatsPage compose_provisions(const ZStatsInput &in) {
    ZStatsPage out{};
    out.kind = ZStatsPageKind::Provisions;
    out.page = kZStatsPageProvisions;
    const auto &g = *in.game;
    // The banner of draw_provisions is DS 0x9724, "Equipment".
    std::snprintf(out.banner, sizeof(out.banner), "Equipment");

    char value[8]{};
    format_number(value, sizeof(value), g.food, 4, ' ');
    // DS 0x972e and 0x9738 are " Food: " and " Gold: " -- the trailing space is
    // part of the string, so the 4-wide value column starts one cell later
    // than the dotted counters' does.
    std::snprintf(out.rows[1].text, sizeof(out.rows[1].text), " Food: %s", value);
    format_number(value, sizeof(value), g.gold, 4, ' ');
    std::snprintf(out.rows[2].text, sizeof(out.rows[2].text), " Gold: %s", value);
    // The dot leaders are embedded in the DGROUP strings themselves (0x9742
    // "Keys......." and 0x9752 "Gems......." carry seven dots, 0x9760
    // "Torches...." four): they align the value column on cell 14.
    format_number(value, sizeof(value), g.keys, 2, ' ');
    std::snprintf(out.rows[4].text, sizeof(out.rows[4].text), " Keys.......%s", value);
    format_number(value, sizeof(value), g.gems, 2, ' ');
    std::snprintf(out.rows[5].text, sizeof(out.rows[5].text), " Gems.......%s", value);
    format_number(value, sizeof(value), g.torches, 2, ' ');
    std::snprintf(out.rows[6].text, sizeof(out.rows[6].text), " Torches....%s", value);
    out.row_count = 7;
    // The grapple line (0x976e) is printed only when one is carried.
    if (g.grapple) {
        put(out.rows[7], " Grapple");
        out.row_count = 8;
    }
    return out;
}

ZStatsPage compose_list(const ZStatsInput &in, int page, size_t scroll) {
    ZStatsPage out{};
    out.kind = ZStatsPageKind::List;
    out.page = page;
    const auto list = zstats_list(in, page);
    std::snprintf(out.banner, sizeof(out.banner), "%.*s", int(sizeof(out.banner) - 1), list.title);
    out.list_total = list.count;

    if (!list.count) {
        // An empty list is DS 0x9794, "(None owned!)" -- render_item_list
        // 0x06e8 prints it inside the frame and waits for a key.
        put_centred(out.rows[0], "(None owned!)");
        out.row_count = 1;
        return out;
    }
    out.list_scroll = std::min(scroll, zstats_max_scroll(list.count));
    const size_t visible = std::min(kZStatsListRows, list.count - out.list_scroll);
    for (size_t i = 0; i < visible; ++i)
        format_list_row(out.rows[i], list.entries[out.list_scroll + i]);
    out.row_count = visible;
    out.more_above = out.list_scroll > 0;
    out.more_below = out.list_scroll + visible < list.count;
    return out;
}

} // namespace

ZStatsPageKind zstats_page_kind(int page) {
    if (page == kZStatsPageProvisions) return ZStatsPageKind::Provisions;
    if (page >= kZStatsPageReagents && page <= kZStatsPageArmaments) return ZStatsPageKind::List;
    return (page & 1) ? ZStatsPageKind::Arms : ZStatsPageKind::Stats;
}

int zstats_member_of_page(int page) {
    if (page < 0 || page >= kZStatsPageProvisions) return -1;
    return page >> 1;
}

int zstats_page_for_member(int member) { return member > 0 ? member * 2 : 0; }

// The forward gates of cmd_zstats 0x0aea: at the last member's Arms page jump
// the unused window to Provisions (0x0af7); at the last list wrap to 0
// (0x0b0a); otherwise step.
int zstats_axis_next(int page, int party_size) {
    const int party = std::max(1, std::min(party_size, int(kMaxParty)));
    if (page >= kZStatsPageArmaments) return 0;
    if (page == party * 2 - 1) return kZStatsPageProvisions;
    if (page < 0) return 0;
    return page + 1;
}

// The backward gates of 0x0aa6: Provisions steps back to the last member's
// Arms page (0x0aac) and slot 0 wraps to the last list (0x0ac6).
int zstats_axis_prev(int page, int party_size) {
    const int party = std::max(1, std::min(party_size, int(kMaxParty)));
    if (page <= 0) return kZStatsPageArmaments;
    if (page == kZStatsPageProvisions) return party * 2 - 1;
    return page - 1;
}

// A roster that shrank while the modal was open can leave the axis inside the
// unused window [party*2, 0x0b], which the ring itself never enters.  Fold it
// back onto the nearest live per-member page rather than stranding the view.
int zstats_clamp_page(int page, int party_size) {
    const int party = std::max(1, std::min(party_size, int(kMaxParty)));
    if (page < 0) return 0;
    if (page > kZStatsPageArmaments) return kZStatsPageArmaments;
    if (page >= party * 2 && page < kZStatsPageProvisions) return party * 2 - 1;
    return page;
}

const char *zstats_class_name(char code) { return class_name_or_null(code); }
const char *zstats_status_name(char code) { return status_name_or_null(code); }

size_t zstats_max_scroll(size_t total) {
    return total > kZStatsListRows ? total - kZStatsListRows : 0;
}

ZStatsList zstats_list(const ZStatsInput &in, int page) {
    if (!in.game) return {};
    const auto &g = *in.game;
    switch (page) {
    // Titles are the DGROUP banners 0x97ac / 0x97b6 / 0x97be / 0x97c4.  List
    // 0xe is SPELLS (the mixture counts at 0x57f0), confirmed by the dumped
    // name table at DS 0x19e2 -- not a second item page.
    case kZStatsPageReagents:
        return build_quantity_list("Reagents", g.reagent_quantities, 8, reagent_display_name);
    case kZStatsPageSpells:
        return build_quantity_list("Spells", g.spell_quantities, 48, spell_display_name);
    case kZStatsPageItems:
        return build_items_list(in);
    case kZStatsPageArmaments:
        return build_quantity_list("Armaments", g.equipment_quantities,
                                   std::min<size_t>(48, g.equipment_count),
                                   equipment_display_name);
    default:
        return {};
    }
}

ZStatsPage compose_zstats_page(const ZStatsInput &in, int page, size_t scroll) {
    if (!in.game) return {};
    switch (zstats_page_kind(page)) {
    case ZStatsPageKind::Stats: return compose_stats(in, page);
    case ZStatsPageKind::Arms: return compose_arms(in, page);
    case ZStatsPageKind::Provisions: return compose_provisions(in);
    case ZStatsPageKind::List: return compose_list(in, page, scroll);
    }
    return {};
}

} // namespace openu5
