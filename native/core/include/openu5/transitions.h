#pragma once
#include "movement.h"
#include "npc_path.h"
namespace openu5 {
// Reload services own existing terrain/object/door systems, just as turn hooks
// own enemy/combat effects. Invoked synchronously in authoritative Game order.
enum class ReloadEffect : uint8_t {
    EnterNpcs,
    ClearEnemies,
    ResetDoors,
    ClearTerrain,
    HydrateInterior,
    RefreshHourTiles,
    UrbanPresence,
    UrbanEffects,
    DiscardInterior,
    HydrateUnderworld,
    ContextTurn
};
struct TransitionServices {
    void *context = nullptr;
    void (*effect)(void *, ReloadEffect, uint8_t) = nullptr;
    void (*event)(void *, GameEventKind, const char *) = nullptr;
};
struct TravelState {
    bool volatile_terrain_wipe = false, drunk_pre_rolled = false;
    int8_t shadowlord_here = -1;
};
struct LocationTable {
    const uint8_t *x = nullptr, *y = nullptr;
    size_t x_count = 0, y_count = 0;
};
int32_t location_at(LocationTable, int32_t x, int32_t y);
bool floor_exists(const WorldData &, MapId);
// Banner is the already localized Game.locationNameBanner result, or nullptr.
Error load_small_map(GameState &, TurnState &, TravelState &, uint8_t id, const char *banner,
                     TransitionServices);
Error exit_to_overworld(GameState &, LocationTable, TransitionServices);
Error confirm_town_exit(GameState &, bool yes, LocationTable, TransitionServices);
bool apply_stair_step(GameState &, const WorldData &, int32_t tile, Direction, TransitionServices);
bool klimb_ladder(GameState &, const WorldData &, int16_t delta, TransitionServices);
struct Moonstone {
    uint8_t x = 0, y = 0;
    int16_t z = 0; // Interior basements use -1; underworld uses 255.
    uint8_t location = 255;
    bool buried = false;
};
bool moonstone_teleport(GameState &, TurnState &, TravelState &, const Moonstone *, size_t count,
                        int32_t phase, const char *banner, TransitionServices);
// Generic movement boundary emits the prompt; accepted/rejected response is
// confirm_town_exit. Position and RNG do not change while awaiting the response.
bool local_boundary(const ActiveMap &, Position, Direction, TransitionServices);
// Bind this hook into TownTurnContext.after_housekeeping; uses the same NPC
// implementation and caller-owned scratch. No device loop change is necessary.
struct ScheduledTurnBinding {
    NpcActors &actors;
    const GameState &state;
    const WorldData &world;
    NpcScanGrid &scratch;
    Rand rand;
    ActorError error = ActorError::None;
    void *map_context = nullptr;
    ActiveMap (*effective_map)(void *, MapId) = nullptr;
};
TurnHook scheduled_npc_hook(ScheduledTurnBinding &);
} // namespace openu5
