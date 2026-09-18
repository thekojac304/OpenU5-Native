#pragma once
#include "turn.h"
namespace openu5 {
struct LootGrant {
    int32_t id = 0, quantity = 0;
};
// The legacy loot record stores an object category in `id` and has a
// category-specific payload in `quantity`; it is not a universal item id.
enum class LootCategory : uint8_t {
    None, Chest, Gold, Potion, Scroll, Equipment, Keys, Gems, Torches, Food,
    QuestItem
};
struct DecodedLootItem {
    LootCategory category = LootCategory::None;
    int32_t item_index = -1, quantity = 0;
};
struct LootSink {
    void *context = nullptr;
    void (*grant)(void *, LootGrant) = nullptr;
};
struct ChestTrap {
    const char *message = "";
    uint8_t damage_mask = 0;
};
ChestTrap chest_trap(GameState &, int location, int opener, Rand);
void chest_loot(int contents, Rand, LootSink);
Error dungeon_chest_loot(int floor, Rand, LootGrant (&out)[7], uint8_t &count);
DecodedLootItem decode_loot(LootGrant);
const char *loot_category_name(LootCategory);
// Returns true only when a valid record changes authoritative inventory.
// Callers retain the loose object when the destination is malformed or full.
bool apply_loot_grant(GameState &, LootGrant);
const char *loot_open_line(int id);
void loot_item_name(LootGrant, char *buffer, size_t capacity);
} // namespace openu5
