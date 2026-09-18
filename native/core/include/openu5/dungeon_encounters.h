#pragma once
#include "combat.h"
namespace openu5 {
struct DungeonArena { const CombatMap *map=nullptr; const uint8_t *sprites=nullptr; };
// Borrowed assets and the existing combat owner. Allocate this scratch owner on
// the heap (or alongside the caller's external arenas), not a device task stack.
struct DungeonEncounters {
    CombatContext *combat=nullptr;
    const DungeonArena *arenas=nullptr;
    size_t count=0;
    CombatMap corridor;
    uint8_t sprites[16]{};
    CombatField fields[16]{};
};
CombatResult dungeon_encounter(CommandContext &,int32_t room_map,bool attack=false);
}
