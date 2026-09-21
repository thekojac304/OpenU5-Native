#pragma once
#include "dungeon.h"
#include "state.h"
#include "world.h"
#include <cstdint>

namespace openu5 {

// ---------------------------------------------------------------------------
// R-17 / Y-14 -- the (V)iew-a-gem SEMANTIC layer.
//
// The original does not paint the gem view from raw tile bits: `gem_view`
// (LOOKOBJ.OVL 0x10fc) classifies every tile through a static per-tile
// CATEGORY table (`draw_gem_map_tile` 0xf7e, byte[tile+0x1d1a]) before a
// jump-table picks a pattern. This header ports that classification and the
// view's geometry (chunk-anchored overworld window, fixed town window,
// dungeon flood-fill) so a host test can prove it without any device pixel
// code. The T-Deck renderer (native_renderer.cpp) consumes `GemView` for
// painting only; it does not re-derive category or geometry.
//
// Authoritative model, in evidence order:
//   * LOOKOBJ.OVL `gem_view` 0x10fc / `draw_gem_map_tile` 0xf7e (overworld);
//   * DNGLOOK.OVL 0x06a8 driver / `draw_gem_map_tile` 0x0340 (dungeon);
//   * the project reference implementation `game/src/core/world/gem-view.ts`
//     (`buildGemView`), `game/src/core/world/chunk-origin.ts`
//     (`initChunkOrigin`) and `game/src/skin/fiel/gemmap-overworld.ts`
//     (`GEM_CATEGORY`).
// ---------------------------------------------------------------------------

constexpr int kGemWindow = 32;                  // overworld/town chunk (gem_view 32x32 loop)
constexpr int kGemDungeonDisplay = 22;          // DNGLOOK display window (flood-fill)
constexpr int kGemDungeonCenter = 11;           // party's fixed display cell
constexpr uint8_t kGemDungeonUnreached = 0xff;  // never-visited display cell

/**
 * Terrain CATEGORY (0-16) for an overworld/town tile id, ported byte-for-byte
 * from GEM_CATEGORY (LOOKOBJ 0xf88, DATA.OVL 0x1d2a / gemmap-overworld.ts).
 * Category 0 is the original's void/no-op branch (nothing drawn).
 */
uint8_t gem_terrain_category(uint8_t tile);

struct GemChunkOrigin {
    int32_t x = 0, y = 0;
};

/**
 * Entry-formula chunk origin (`initChunkOrigin`, MAINOUT.OVL 0x0019-0x004c):
 * aligns the 32x32 gem window to the 16-cell block containing the party,
 * shifted one block toward the origin when the party sits in the low half of
 * its block -- so the window is ANCHORED to the block, never centred on the
 * party. The original also keeps this origin hysteretically while walking
 * (`scrollChunkOrigin`); this port uses only the stateless entry formula,
 * which the reference itself treats as an accepted per-render fallback
 * (chunk-origin.ts `gemChunkOrigin`, used whenever no persisted origin is
 * fresh) and which already reproduces the essential, testable property: not
 * centred, quantized to 16. Reproducing the full per-step hysteresis would
 * require new persistent overworld-movement state, out of this batch's scope.
 */
GemChunkOrigin gem_chunk_origin(uint8_t party_x, uint8_t party_y);

struct GemCell {
    /** overworld/town: terrain category 0-16. dungeon: high-nibble cell type,
     *  or kGemDungeonUnreached for a display cell the flood never reached. */
    uint8_t value = 0;
    /** dungeon only: low-nibble subtype (e.g. wall density). Always 0 for the
     *  overworld/town variant. */
    uint8_t sub = 0;
};

struct GemView {
    bool dungeon = false;
    uint8_t width = 0, height = 0;
    /** [row][col]; the dungeon variant only uses the top-left 22x22. */
    GemCell cells[kGemWindow][kGemWindow]{};
    uint8_t marker_x = 0, marker_y = 0;
};

/**
 * Builds the overworld/town gem view -- `buildGemView`'s non-dungeon branch.
 * A large (wrapping) map uses the chunk-origin-anchored window with toroidal
 * wrap; a small map (town/castle) shows the WHOLE fixed 32x32 map at (0,0),
 * exactly as the original never scrolls a town's gem view. Always fills every
 * one of the 32x32 cells -- the full-square contract Y-14 requires.
 */
GemView build_world_gem_view(const ActiveMap &map, Position party);

/**
 * Builds the dungeon gem view -- `buildGemView`'s dungeon branch / DNGLOOK
 * 0x06a8: an 8-connected flood fill over the 22x22 DISPLAY window, seeded at
 * the centre (the party), reading the 8x8 floor toroidally. A wall/special-
 * wall/secret-door cell (type 0xb/0xc/0xd) is drawn as the flood's border but
 * does not propagate; every other cell propagates, INCLUDING normal doors and
 * rooms. Unlike movement passability, this never consults `revealed` -- the
 * gem always shows secrets, discovered or not.
 */
GemView build_dungeon_gem_view(const DungeonState &dungeon);

} // namespace openu5
