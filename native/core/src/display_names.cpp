#include "openu5/display_names.h"

#include "openu5/magic.h"
#include <cstring>

namespace openu5 {
namespace {
template <size_t N> const char *at(const char *const (&values)[N], int32_t id) {
    return id >= 0 && size_t(id) < N && values[id] && *values[id] ? values[id] : nullptr;
}

// The 48 equipment names of DATA.OVL DS 0x17f6 (the DGROUP long-name pointer
// table; the reference ships the byte-exact dump as
// game/src/core/data/longEquipNames.json).  The index IS the save-game
// equipment id -- the same id space as inventory.cpp's `types`/`weights`
// tables, which are verbatim copies of DS 0x1a7e / DS 0x1aae.
//
// R-22 CORRECTION (Batch 14).  This table was previously transcribed from
// InventoryDetails.json's `Armament` map, which carries a PHANTOM
// `[0] = "BareHands"` entry from the clone-era Redux data set, so every name
// sat one slot high.  The reference itself compensates with `.slice(1)`
// (game/src/ui/shop-console.ts, and the rule is spelled out in
// game/src/core/world/search.ts's EQUIP_IDX_GLASS_SWORD comment); native did
// not, so `equipment_display_name(16)` returned "Mystic Armour" for the
// Dagger and `(27)` returned "Bow" for the Arrows.  Three independent
// anchors inside native itself pin the correct alignment:
//   * equip_type_of(9..15) == 0x40 -- the armour band ZSTATS.OVL:0x0c94
//     locks in battle, so id 9 is an armour, not the Jewel Shield;
//   * ammo_item_for() maps 26/36 -> 27 and 28 -> 29 -- Bow/Magic Bow ->
//     Arrows and Crossbow -> Quarrels (ZSTATS.OVL:0x0d0c);
//   * is_thrown_weapon() is {16, 21, 22} -- Dagger/Spear/Throwing Axe
//     (COMSUBS:0x097c, rama 0x9b8).
// Every caller (Z-stats Arms and Armaments pages, the Ready picker, the
// blacksmith, the Developer equipment rows) was showing the neighbouring
// item's name before this correction.
constexpr const char *equipment_names[] = {
    "Leather Helm", "Chain Coif", "Iron Helm", "Spiked Helm",
    "Small Shield", "Large Shield", "Spiked Shield", "Magic Shield", "Jewel Shield",
    "Cloth Armour", "Leather Armour", "Ring Mail", "Scale Mail", "Chain Mail",
    "Plate Mail", "Mystic Armour", "Dagger", "Sling", "Club", "Flaming Oil",
    "Main Gauche", "Spear", "Throwing Axe", "Short Sword", "Mace", "Morning Star",
    "Bow", "Arrows", "Crossbow", "Quarrels", "Long Sword", "2H Hammer", "2H Axe",
    "2H Sword", "Halberd", "Sword of Chaos", "Magic Bow", "Silver Sword", "Magic Axe",
    "Glass Sword", "Jeweled Sword", "Mystic Sword", "Ring of Invisibility",
    "Ring of Protection", "Ring of Regeneration", "Amulet/Turning", "Spiked Collar",
    "Ankh"
};
static_assert(sizeof(equipment_names) / sizeof(equipment_names[0]) == 48,
              "the equipment name table must cover exactly the 48 ids of DS 0x1a7e");
constexpr const char *reagent_names[] = {
    "Sulfur Ash", "Ginseng", "Garlic", "Spider Silk", "Blood Moss", "Black Pearl",
    "Nightshade", "Mandrake"
};
constexpr const char *potion_names[] = {
    "Blue Potion", "Yellow Potion", "Red Potion", "Green Potion", "Orange Potion",
    "Purple Potion", "Black Potion", "White Potion"
};
constexpr const char *scroll_names[] = {
    "Vas Lor Scroll", "Rel Hur Scroll", "In Sanct Scroll", "In An Scroll",
    "In Quas Wis Scroll", "Kal Xen Corp Scroll", "In Mani Corp Scroll", "An Tym Scroll"
};
// (U)se picker / extended-item table names, indexed by the REAL canonical item
// id used by CommandKind::UseItem and by AlphaRuntime's Use picker -- the
// ZSTATS extended item table 0xB9EE id space the reference dispatches on
// (game/src/core/endgame/use-tools.ts header): 0x10 Carpet, 0x11 Skull Key,
// 0x12 Amulet, 0x13 Crown, 0x14 Sceptre, 0x15-0x1c Moonstones, 0x1d-0x1f
// Shards, 0x20 Spyglass, 0x21 HMS Cape plans, 0x22 Sextant, 0x23 Pocket Watch,
// 0x24 Black Badge, 0x25 Wooden Box.  There is exactly ONE interpretation of
// this index: the real id.  The former alternate "offset" reading (slot 0 =
// Carpet, 1 = Skull Key, 2 = Grapple) is gone -- Grapple is a Klimb-only item
// and is not part of the (U)se table at all (R-07).
//
// Ids 0-15 are the scroll (0x00-0x07) and potion (0x08-0x0f) ranges, which
// AlphaRuntime::open_selection() names through scroll_display_name() /
// potion_display_name(); they are deliberately null here.
//
// Id 35 (0x23, Pocket Watch) stays null: no GameState field backs it, so the
// picker cannot gate a row on possession (R-08 / audit section F).  Naming it
// here would let an ungated row render.
//
// Moonstones share one generic name: the reference's name-table entry is the
// format sigil "(N" that prints "Moonstone " plus the phase digit
// (game/src/core/usePicker.ts::useMoonstoneRowName), not eight distinct names.
constexpr const char *usable_names[] = {
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    "Magic Carpet",            // 16 / 0x10
    "Skull Key",               // 17 / 0x11
    "Amulet of Lord British",  // 18 / 0x12
    "Crown of Lord British",   // 19 / 0x13
    "Sceptre of Lord British", // 20 / 0x14
    "Moonstone", "Moonstone", "Moonstone", "Moonstone", // 21-24 / 0x15-0x18
    "Moonstone", "Moonstone", "Moonstone", "Moonstone", // 25-28 / 0x19-0x1c
    "Shard of Falsehood",      // 29 / 0x1d
    "Shard of Hatred",         // 30 / 0x1e
    "Shard of Cowardice",      // 31 / 0x1f
    "Spyglass",                // 32 / 0x20
    "HMS Cape Plans",          // 33 / 0x21
    "Sextant",                 // 34 / 0x22
    nullptr,                   // 35 / 0x23 Pocket Watch -- no backing state
    "Black Badge",             // 36 / 0x24
    "Wooden Box"               // 37 / 0x25
};
constexpr const char *guild_names[] = {"Keys", "Gems", "Torches"};
constexpr const char *ship_names[] = {"Ship", "Skiff"};
constexpr const char *wine_names[] = {
    "Ale", "Beer", "Wine", "Mead", "Rum", "Spirits"
};
constexpr const char *generic_prefixes[] = {
    "Equipment ", "Item ", "Object ", "Spell ", "Reagent ", "NPC ", "Enemy ",
    "Shop ", "Location "
};
}

const char *equipment_display_name(int32_t id) { return at(equipment_names, id); }
const char *reagent_display_name(int32_t id) { return at(reagent_names, id); }
const char *potion_display_name(int32_t id) { return at(potion_names, id); }
const char *scroll_display_name(int32_t id) { return at(scroll_names, id); }
const char *usable_item_display_name(int32_t id) { return at(usable_names, id); }
const char *guild_item_display_name(int32_t id) { return at(guild_names, id); }
const char *ship_service_display_name(int32_t id) { return at(ship_names, id); }
const char *wine_display_name(int32_t id) { return at(wine_names, id); }
const char *spell_display_name(int32_t id) {
    const auto *definition = id >= 0 && id < 48 ? spell_definition(SpellId(id)) : nullptr;
    return definition && definition->name && *definition->name ? definition->name : nullptr;
}

bool is_generic_identifier_label(const char *text) {
    if (!text) return false;
    for (const char *prefix : generic_prefixes) {
        const auto n = std::strlen(prefix);
        if (std::strncmp(text, prefix, n) == 0 && text[n] >= '0' && text[n] <= '9') return true;
    }
    return false;
}

} // namespace openu5
