#pragma once
#include "transport.h"
#include <vector>
namespace openu5 {
// Single owner of terrain overrides. Static maps are borrowed; all variable
// storage is heap-backed. Hour/volatile layers are residency state, not saves.
struct TerrainCell { MapId map{}; int32_t x=0,y=0,tile=0; };
// A terrain override is an authoritative world mutation. The optional
// observer is owned by the platform/runtime, keeping the core device-neutral.
struct TerrainWrite { MapId map{}; int32_t x=0,y=0,old_tile=kOffMap,new_tile=kOffMap; bool permanent=false; const char *caller="WorldTerrain::set"; };
struct TerrainWriteObserver { void *context=nullptr; void (*emit)(void *,const TerrainWrite &)=nullptr; };
struct TerrainSample {
    int32_t base=kOffMap, hourly_tile=kOffMap, persistent_tile=kOffMap,
            transient_tile=kOffMap, effective=kOffMap;
    bool hourly=false,persistent=false,transient=false,wiped=false;
};
struct WorldTerrain {
    std::vector<TerrainCell> persistent, transient, hourly;
    bool wiped=false;
    MapId wipe_map{};
    int32_t wipe_tile=0;
    TerrainWriteObserver writes{};
    int32_t raw(const WorldData &,MapId,int32_t,int32_t) const;
    int32_t effective(const WorldData &,MapId,int32_t,int32_t) const;
    TerrainSample inspect(const WorldData &,MapId,int32_t,int32_t) const;
    void set(MapId,int32_t,int32_t,int32_t,bool permanent=false,const char *caller="WorldTerrain::set");
    void remove_boarded(MapId,int32_t,int32_t,int32_t);
    void clear_residence();
    void refresh(const WorldData &,const GameState &);
};
// These adapters operate on the existing QuestWorldServices object owner and
// WorldTerrain. Keep the CommandContext at a stable address while bound.
TransportServices world_transport_services(CommandContext &);
// Batch 53 (H-146 / RB-4). A shop's transport sale, over the SAME two owners
// world_transport_services() manages, so a bought transport is boarded,
// parked and saved like any other:
//   reserve  -- the transport reservation itself (one more persistent cell;
//               one more object for a ship);
//   ship     -- the reference's spawnDockShip(dockX, dockY, flags, 0): an
//               object on the overworld dock (location 0, the party's floor)
//               carrying the tile, hull 99 and skiffs of MAINOUT 0x0D22;
//   horse    -- the reference's stableHorse: the persistent 0x110 override
//               beside the party (SHOPPES 0x0959), which board/remove_boarded
//               already read as a horse.
bool reserve_world_transport(CommandContext &, bool ship);
void place_purchased_ship(CommandContext &, int32_t x, int32_t y, int32_t tile, int32_t hull, int32_t skiffs);
void place_purchased_horse(CommandContext &, int32_t x, int32_t y);
constexpr int32_t kPurchasedHorseTile = 0x110;
}
