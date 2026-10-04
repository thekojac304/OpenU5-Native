#pragma once
#include "state.h"
#include <array>
#include <cstddef>

namespace openu5 {
// Optional TS fields stay separate from the device's foundation projection.
// Caller owns this state and the RNG; no default/global stream is introduced.
struct TurnState {
    int32_t prev_hour = 0, light_spell_minutes = 0, drunk_turns = 0;
    char time_spell = 0;
    int32_t spell_turns = -1; // -1 = undefined; 255 = permanent.
    int32_t transport_tile = 0x1c, wind = 0, wind_drift_counter = 0;
    int32_t sail_dir = 0, hms_cape_toggle = 0;
    bool has_shadowlords = false;
    std::array<int32_t, 3> shadowlord_locations{{128,128,128}};
    int32_t skull_tree_day = 0;
    std::array<int32_t, 3> reagent_days{};
    int32_t felucca_phase = 0x30, trammel_phase = 0x30;
    // A4-PARITY2 D-88: g_cmb_action_count, DS:0x5882 = SAVED.GAM +0x2DC (u8). COMBAT.OVL 0x0C64-0x0C76 counts every unit
    // ACTIVATION and at 10 zeroes it and calls advance_clock(1). NOT combat-local: nothing initialises it at combat entry
    // or exit; it lives in the saved window and survives fights, saves and loads (only a fresh INIT.GAM starts at 0).
    uint8_t combat_clock = 0;
};
struct Rand {
    void *context;
    int32_t (*draw)(void *, int32_t, int32_t);
    int32_t operator()(int32_t lo, int32_t hi) const { return draw(context, lo, hi); }
};
Rand rng_source(OriginalRng &rng);
struct SkyRefresh {
    const int32_t *phases = nullptr;
    size_t count = 0;
    int32_t location = 0;
};
struct TurnPhases { int32_t mount = 0, quickness = 0; };
enum class TurnMessage : uint8_t { Starving, Hic, Trapdoor, Burning };
const char *turn_message_text(TurnMessage message);
struct MemberIndices { std::array<uint8_t, 6> values{}; uint8_t count = 0; };
struct SpawnRoll { int32_t roll = 0, threshold = 0; bool spawn = false; };
struct TrollRoll {
    bool fired = false, on_foot = false, runs_inner_world_turn = false;
    MemberIndices indices{};
    std::array<int32_t, 6> dex_rolls{};
    int32_t payer_index = -1, toll = 0;
};
// Streaming trace has no fixed-size truncation and no heap allocation.
struct TraceSink {
    void *context = nullptr;
    void (*emit)(void *, const char *, int32_t, int32_t, int32_t) = nullptr;
};
struct TurnResult {
    std::array<TurnMessage, 12> messages{}; // Max nine trapdoor messages + housekeeping.
    uint8_t message_count = 0;
    MemberIndices poisoned{}, poison_ticks{};
    bool has_spawn = false, has_troll = false, hazard = false, burning = false;
    SpawnRoll spawn{};
    TrollRoll troll{};
};
struct TurnHook {
    void *context = nullptr;
    void (*call)(void *) = nullptr;
    void operator()() const { if (call) call(context); }
};
enum class TrapdoorOutcome : uint8_t { None, Fell, PartyKilled };
struct OutdoorTurnContext {
    int32_t tile_under_party = 0, minutes = 2;
    bool blocked = false, on_bridge = false, on_swamp = false, skip_world_turn = false;
    const SkyRefresh *sky = nullptr;
    TurnHook after_wind{};
};
struct TownTurnContext {
    bool consumes_turn = true, confused = false, pre_rolled = false;
    bool damage_tile = false, on_swamp_tile = false, second_world_turn = false, pass_command = false;
    const SkyRefresh *sky = nullptr;
    TurnPhases *npc_phases = nullptr;
    TurnHook after_housekeeping{};
    void *hazard_context = nullptr;
    int32_t (*tile_under_party)(void *) = nullptr;
    TrapdoorOutcome (*on_trapdoor)(void *) = nullptr;
};
bool refresh_moon_phase_latch(GameState &, TurnState &, const SkyRefresh &, bool hour_changed);
// `party_location` is g_location as the midnight Shadowlord re-roll sees it (0x5011): INT32_MIN = the live position;
// inside an arena the binary has 0xFF there (A4-PARITY2 D-88), so no town is excluded.
void advance_clock(GameState &, TurnState &, int32_t minutes, const Rand *rand = nullptr, const SkyRefresh *sky = nullptr,
                   int32_t party_location = INT32_MIN);
void apply_damage(GameState &, int32_t index, int32_t amount);
// A4-ENH2: `starvation` -- the hunger site only -- scales each HP loss by the
// difficulty's starvation share (rules_starvation_damage); the draws are the same.
void party_random_damage(GameState &, Rand, bool starvation = false);
TurnResult turn_housekeeping(GameState &, TurnState &, Rand);
TurnResult advance_turn(GameState &, TurnState &, int32_t minutes, Rand, const SkyRefresh *sky = nullptr);
bool maybe_change_wind(TurnState &, Rand);
int32_t spawn_threshold(int32_t tile, int32_t floor, int32_t hour);
SpawnRoll roll_spawn_gate(Rand, int32_t tile, int32_t floor, int32_t hour);
bool town_npc_tail_runs(const TurnState &, TurnPhases &, bool pass_command);
bool outdoor_world_turn_runs(const GameState &, const TurnState &, TurnPhases &);
TurnResult outdoor_turn(GameState &, TurnState &, Rand, const OutdoorTurnContext &, TraceSink = {});
TurnResult town_turn(GameState &, TurnState &, Rand, const TownTurnContext &, TraceSink = {});
}
