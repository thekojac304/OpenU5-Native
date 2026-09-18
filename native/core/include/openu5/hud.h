#pragma once
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
}
