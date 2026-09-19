#include "openu5/display_names.h"

#include "openu5/magic.h"
#include <cstring>

namespace openu5 {
namespace {
template <size_t N> const char *at(const char *const (&values)[N], int32_t id) {
    return id >= 0 && size_t(id) < N && values[id] && *values[id] ? values[id] : nullptr;
}

// InventoryDetails.json Armament ids, normalized to the human-readable names
// extracted from DATA.OVL.  The index is the save-game equipment id.
constexpr const char *equipment_names[] = {
    "Bare Hands", "Leather Helm", "Chain Coif", "Iron Helm", "Spiked Helm",
    "Small Shield", "Large Shield", "Spiked Shield", "Magic Shield", "Jewel Shield",
    "Cloth Armour", "Leather Armour", "Ring Mail", "Scale Mail", "Chain Mail",
    "Plate Mail", "Mystic Armour", "Dagger", "Sling", "Club", "Flaming Oil",
    "Main Gauche", "Spear", "Throwing Axe", "Short Sword", "Mace", "Morning Star",
    "Bow", "Arrows", "Crossbow", "Quarrels", "Long Sword", "2H Hammer", "2H Axe",
    "2H Sword", "Halberd", "Sword of Chaos", "Magic Bow", "Silver Sword", "Magic Axe",
    "Glass Sword", "Jeweled Sword", "Mystic Sword", "Ring of Invisibility",
    "Ring of Protection", "Ring of Regeneration", "Amulet of Turning", "Spiked Collar"
};
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
