#pragma once

#include <cstddef>
#include <cstdint>

#include "combat.h"
#include "commands.h"

namespace openu5 {

constexpr int kPresentationWindow = 11;
constexpr int kPresentationCells = kPresentationWindow * kPresentationWindow;
constexpr int16_t kPresentationOffMap = -1;
constexpr int16_t kPresentationHidden = -2;

enum class TileAnimationKind : uint8_t {
    Static,
    TileCycle,
    WaterScroll,
    WaterComposite,
    FireNoise,
    PerTurnCycle,
    ActorProgram,
};

struct PresentationSnapshot {
    int16_t tiles[kPresentationCells]{};
    uint8_t visible[kPresentationCells]{};
    uint8_t animated[kPresentationCells]{};
    uint32_t actor_ids[kPresentationCells]{};
    uint8_t actor_seeds[kPresentationCells]{};
    Position center{};
    int8_t active_x = -1, active_y = -1;
    int8_t target_x = -1, target_y = -1;
    bool combat = false;
    bool active_enemy = false;
    bool target_valid = true;
    bool any_animated = false;
};

// Stateful presentation-only interpreter for the reference actor animation
// programs.  It owns a local view PRNG and never touches gameplay RNG/state.
class ActorAnimationClock {
  public:
    void render(PresentationSnapshot &, uint32_t presentation_phase, bool frozen = false);
    void reset();
  private:
    struct Entry {
        uint32_t id = 0;
        uint16_t bank = 0, frame = 0;
        uint8_t base = 0, seed = 0, pc = 0, timer = 0;
        bool live = false;
    };
    Entry entries_[64]{};
    uint32_t prng_ = 0x5c5a1d1eU, last_phase_ = 0;
    bool phase_initialized_ = false;
    uint8_t random_byte();
    void tick();
};

/** The same light-radius calculation used by the reference CoreView. */
int32_t presentation_light_level(const GameState &, const TurnState &);

/**
 * Y-04 (Quake): vertical viewport-shake offset in canvas px at time `t_ms`
 * since the shake started, for a shake of `pulses` pulses (default one
 * kernel-primitive invocation; the Codex ceremony triggers three in a row).
 * Square wave -- `kQuakeAmplitudePx` during the down phase of each pulse,
 * 0 during rest, 0 outside [0, pulses*kQuakePeriodMs). Pure function (host
 * test coverage); the device applies it as a re-blit offset of the 11x11
 * game window only -- HUD/frame/text never move (skin/fiel/quake.ts, the
 * reference's own witness-derived dynamics).
 */
constexpr int kQuakeAmplitudePx = 2;
constexpr int kQuakePulses = 8;
constexpr int kQuakeDownMs = 42;
constexpr int kQuakePeriodMs = 117; // down(42) + rest(75)
constexpr int kQuakeDurationMs = kQuakePulses * kQuakePeriodMs;
int quake_offset_at(int32_t t_ms, int pulses = kQuakePulses);

/** Classify a tile without advancing gameplay state or consuming gameplay RNG. */
TileAnimationKind tile_animation_kind(int32_t tile);

/** Select the logical tile-id frame. phase is the free-running 55 ms presentation tick. */
int32_t animated_tile_frame(int32_t tile, uint32_t phase, int64_t world_turn);

/**
 * Compose terrain, live overrides, objects/actors, visibility, and the party.
 * `reveal_all` bypasses the light-radius/sight-blocking censorship entirely
 * (R-12: Wis An Ylem / In Quas Wis / the white potion's Death Vision effect --
 * `revealViewport` in the reference `CoreView`, which returns no censorship
 * bitmap at all while active, not a flood run with light=infinity, because a
 * flood still stops at walls and would leave sealed rooms dark). It never
 * touches GameState/TurnState: the effect is real-time, not turn-gated, so
 * the caller (the device render loop) owns the timer and passes the flag.
 */
PresentationSnapshot compose_world_presentation(CommandContext &, const ActiveMap &,
                                                 Position center, int32_t avatar_tile,
                                                 bool reveal_all = false);

/** Compose the live combat arena, including fields, loot, and combatants. */
PresentationSnapshot compose_combat_presentation(const CombatState &, const GameState &);

/**
 * Convert the combat arena's encoded loot byte into the high-bank display
 * tile. Bit 7 is trap metadata on a chest, not part of the tile id.
 */
int32_t combat_loot_render_tile(int16_t encoded_loot);

} // namespace openu5
