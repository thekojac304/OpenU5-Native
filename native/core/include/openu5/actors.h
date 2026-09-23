#pragma once
#include "turn.h"
#include "world.h"
namespace openu5 {
struct NpcSlot {
    uint8_t slot = 0, type = 0, dialog = 0;
    uint8_t ai[3]{}, x[3]{}, y[3]{}, z[3]{}, times[4]{};
};
struct NpcActor {
    NpcSlot schedule{};
    int16_t x = 0, y = 0, z = 0;
    uint8_t location = 0, state = 1, served_slot = 0;
    int16_t path_index = -1, stuck = 0;
    std::array<uint8_t,32> path{};
};
struct NpcActors { std::array<NpcActor,32> actors{}; size_t count = 0; };
enum class ActorError : uint8_t { None, Capacity, NeedsPathfinding, InvalidMap };
// Fresh entry only. Save restoration is not part of this interface.
ActorError enter_npc_map(NpcActors &, const NpcSlot *, size_t count, uint8_t location, uint8_t hour, uint32_t dead_slots);
int32_t npc_check_schedule(NpcActor &, uint8_t hour, int16_t visible_floor);
// NPC half of TOWN.OVL:0x1694 town_populate_npcs: every live NPC of the
// location to its schedule period for `hour`. A reposition, not a rebuild --
// cleared slots stay gone, the stuck counter is left alone.
void snap_npcs_to_schedule(NpcActors &, uint8_t location, uint8_t hour);
bool npc_occupied(const NpcActors &, const WorldPosition &party, uint8_t location, int16_t floor, int16_t x, int16_t y, uint8_t except_slot);
// Caller supplies the effective map (door overrides already applied, as in TS).
void npc_ai_step(NpcActor &, uint8_t schedule_index, NpcActors &, const GameState &, const ActiveMap &, Rand);
// Exact NpcManager.tick domain with no active/new scheduled walk. Unsupported
// walks return BEFORE mutation/RNG, never silently run simplified pathfinding.
ActorError tick_idle_npcs(NpcActors &, const GameState &, const ActiveMap &, Rand);
ActorError tick_guards(NpcActors &, const GameState &, const ActiveMap &, Rand);
enum class PoolOwnerKind : uint8_t { None, Enemy, Object };
struct PoolEntity { int32_t slot = -1, tile = 0, x = 0, y = 0, floor = 0, location = 0; };
struct PoolSlot { uint8_t tile = 0; int32_t x = 0, y = 0, floor = 0; PoolOwnerKind kind = PoolOwnerKind::None; size_t owner_index = 0; };
using ActorPool = std::array<PoolSlot,32>;
uint8_t find_free_actor_slot(uint32_t occupied);
uint8_t first_free_recycle_slot(uint32_t occupied);
uint8_t scan_recyclable_slot(const ActorPool &, int32_t lo, int32_t hi, bool offscreen_only, int32_t px, int32_t py);
uint8_t acquire_actor_slot(const ActorPool &, int32_t px, int32_t py);
ActorPool compose_world_pool(int32_t location, int32_t floor, PoolEntity *enemies, size_t enemy_count, PoolEntity *objects, size_t object_count);
}
