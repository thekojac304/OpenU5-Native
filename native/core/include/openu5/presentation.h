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

/** Classify a tile without advancing gameplay state or consuming gameplay RNG. */
TileAnimationKind tile_animation_kind(int32_t tile);

/** Select the logical tile-id frame. phase is the free-running 55 ms presentation tick. */
int32_t animated_tile_frame(int32_t tile, uint32_t phase, int64_t world_turn);

/** Compose terrain, live overrides, objects/actors, visibility, and the party. */
PresentationSnapshot compose_world_presentation(CommandContext &, const ActiveMap &,
                                                 Position center, int32_t avatar_tile);

/** Compose the live combat arena, including fields, loot, and combatants. */
PresentationSnapshot compose_combat_presentation(const CombatState &, const GameState &);

/**
 * Convert the combat arena's encoded loot byte into the high-bank display
 * tile. Bit 7 is trap metadata on a chest, not part of the tile id.
 */
int32_t combat_loot_render_tile(int16_t encoded_loot);

} // namespace openu5
