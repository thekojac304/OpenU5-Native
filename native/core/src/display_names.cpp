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
constexpr const char *usable_names[] = {
    "Magic Carpet", "Skull Key", "Grapple", nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    "Spyglass", nullptr, "Sextant", nullptr, nullptr, "Wooden Box"
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
