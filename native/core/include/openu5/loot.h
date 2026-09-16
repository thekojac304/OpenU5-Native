#pragma once
#include "turn.h"
namespace openu5 {
struct LootGrant {
    int32_t id = 0, quantity = 0;
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
void apply_loot_grant(GameState &, LootGrant);
const char *loot_open_line(int id);
void loot_item_name(LootGrant, char *buffer, size_t capacity);
} // namespace openu5
