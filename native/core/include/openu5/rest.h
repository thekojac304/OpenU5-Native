#pragma once
#include "commands.h"
namespace openu5 {
struct WorldTerrain;
struct CampCell {
    int32_t col = 0, row = 0;
    bool present = false;
};
struct RestServices {
    void *context = nullptr;
    void (*snap_npcs)(void *) = nullptr;
    bool (*occupied)(void *, int32_t, int32_t, int32_t) = nullptr;
    bool (*cell_free)(void *, int32_t, int32_t) = nullptr;
    CampCell (*guard_start)(void *, int32_t) = nullptr;
    // Borrowed, already quoted KARMA.DAT record; required for camp/wake.
    const char *(*karma_record)(void *, int32_t) = nullptr;
};
struct RestContext {
    GameState &game;
    TurnState &turn;
    Rand rand;
    EventSink events{};
    RestServices services{};
    const SkyRefresh *sky = nullptr;
    WorldTerrain *terrain = nullptr;
    const WorldData *world = nullptr;
};
struct RestEligibility {
    bool ok = true, ship = false, bed = false, in_town = false;
    const char *message = "";
};
struct RestResult {
    bool ambush = false, thrown_out = false, invalid_context = false;
    int32_t enemy = -1;
    CampCell guard{};
};
RestEligibility camp_context(const GameState &, const TurnState &, int32_t tile,
                             bool dungeon = false);
int32_t camp_watch_count(const GameState &);
CampCell camp_guard_walk(CampCell, Rand, RestServices = {});
bool camp_hole_up(GameState &, Rand, int32_t guard = -1);
bool camp_wake(RestContext &,
               int32_t guard = -1); // false, no mutation if record provider is absent
RestResult camp_sleep_step(RestContext &, int32_t hour, int32_t hours, CampCell = {});
RestResult camp(RestContext &, int32_t hours, int32_t guard = -1);
void bed_sleep_begin(RestContext &);
bool bed_sleep_step(RestContext &);
bool bed_sleep_end(RestContext &); // false, no mutation if x+1 is outside byte coordinates
RestResult bed_sleep(RestContext &, int32_t hours);
void camp_repair_ship(RestContext &);
} // namespace openu5
