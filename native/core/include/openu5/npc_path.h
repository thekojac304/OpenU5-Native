#pragma once
#include "actors.h"
namespace openu5 {
// Exact TS raster (including its guard row and x=32 alias), reusable by caller.
using NpcScanGrid = std::array<uint8_t, 1056>;
struct ScanPoint {
    int16_t x = 0, y = 0;
    bool found = false;
};
ScanPoint npc_scan(NpcScanGrid &, int16_t sy, int16_t sx);
size_t npc_backtrace(NpcActor &, int16_t fy, int16_t fx, const NpcScanGrid &);
struct NpcTravelContext {
    const WorldData &world;
    // Effective visible floor, including the caller's DoorManager overrides.
    const ActiveMap &map;
    NpcScanGrid &scratch;
};
ActorError tick_npcs(NpcActors &, const GameState &, const NpcTravelContext &, Rand);
} // namespace openu5
