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
}
