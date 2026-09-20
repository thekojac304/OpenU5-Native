#pragma once
#include "dungeon.h"
#include "state.h"
#include "turn.h"
#include <cstddef>
#include <cstdint>

namespace openu5 {
// Physical 320x240 HUD contract. The lower-left region is intentionally blank;
// future touch controls must translate hit tests here into ordinary UiActions.
constexpr int kHudViewportX=4,kHudViewportY=4,kHudViewportW=176,kHudViewportH=176;
constexpr int kHudTouchX=4,kHudTouchY=184,kHudTouchW=176,kHudTouchH=56;
constexpr int kHudRightX=184,kHudRightY=4,kHudRightW=135,kHudRightH=236;
constexpr int kHudViewportFrameX=2,kHudViewportFrameY=2,kHudViewportFrameW=180,kHudViewportFrameH=180;
constexpr int kHudSkyBarX=kHudViewportX,kHudSkyBarY=kHudViewportY,kHudSkyBarW=kHudViewportW,kHudSkyBarH=9;
constexpr int kHudWindBarX=kHudViewportX,kHudWindBarY=kHudViewportY+kHudViewportH-9,
              kHudWindBarW=kHudViewportW,kHudWindBarH=9;
constexpr int kHudPartyFrameX=182,kHudPartyFrameY=2,kHudPartyFrameW=137,kHudPartyFrameH=52;
constexpr int kHudWorldFrameX=182,kHudWorldFrameY=54,kHudWorldFrameW=137,kHudWorldFrameH=32;
static_assert(kHudPartyFrameX+kHudPartyFrameW-1<=319,"party frame must fit ST7789");
static_assert(kHudWorldFrameX+kHudWorldFrameW-1<=319,"world frame must fit ST7789");
constexpr int kHudTranscriptSeparatorY=86;
constexpr int kHudGlyphWidth=5,kHudGlyphHeight=7,kHudCellWidth=6,kHudCellHeight=8;
constexpr int kHudTranscriptColumns=kHudRightW/kHudCellWidth;
constexpr int kHudTranscriptY=88,kHudTranscriptLines=(240-kHudTranscriptY)/kHudCellHeight;
struct SkyMark {
    int8_t cell = -1;
    uint8_t glyph = 0;
    bool sun = false;
};
struct HudWorldState {
    SkyMark marks[3]{};
    uint8_t mark_count = 0;
    uint8_t felucca = 0, trammel = 0;
    const char *wind = "Calm  Winds";
    bool sky_visible = false, wind_visible = false;
};
HudWorldState hud_world_state(const GameState &, const TurnState &,
                              const int32_t *moon_phases, size_t moon_phase_count,
                              bool dungeon_active = false);

/**
 * R-05 -- the two DUNGEON bands.  The original replaces the overworld's sky and
 * wind strips with the dungeon's own readout while a dungeon view is mounted:
 * the top band is the level ("L1".."L8", g_floor 0..7 + 1) and the bottom band
 * is the facing ("Dir:" + the direction name right-justified in a field of 7),
 * both bracketed.  Reference: skin/fiel/dungeon.ts `dungeonLevelLabel` /
 * `dungeonDirLabel` / `DUNGEON_DIR_NAMES`, painted by skin.ts; the direction
 * literals are the same English table the wind strip uses.
 *
 * On the T-Deck the two bands are drawn INSIDE the 176x176 viewport, because the
 * device frame is 2 px where the original's is 8 -- a deliberate, documented
 * platform divergence.  Their CONTENT, which is what this function owns, is the
 * original's; without it a turn in place produced no on-screen feedback at all
 * in a corridor whose two directions look alike.
 */
struct HudDungeonBands {
    char level[8]{};
    char direction[16]{};
    // Batch 9B.  The dungeon's own identity, for the HUD's location caption.
    // GameState::position keeps the SURFACE return context for the whole
    // session, so the caption cannot be derived from it while this is active.
    uint8_t dungeon_id = 0;
    bool active = false;
};
HudDungeonBands hud_dungeon_bands(const DungeonState &, bool dungeon_active);
}
