#pragma once

#include <cstdint>

#include "openu5/audio.h"

// Alpha 3 A3-03 -- the ambient proximity sounds (fountain, waterfall, clock):
// ULTIMA.EXE ambient_sfx_tick 0x4102, re-read for this batch
// (native/core/a3-03-cue-sites.log; re/notes/ambient-audio-audit.md).
//
//   0x4102-0x4247  scan the 11x11 window around the party ((5,5) in an arena),
//                  x outer, y inner; keep the NEAREST animated object of a
//                  sounding class (squared distance < best, best starts at 0x33)
//   0x4247-0x430e  sound that class (switch on it)
//   0x430e-0x4323  if [0x5884] != 0 and the phase is 0 or 4: [0x5884]--
//   0x4327-0x4337  phase [0x6a34] = (phase + 1) & 7
//
// Cadence (the runtime's side): 0x4102 is called only by viewport_redraw
// 0x5910, which the key wait getkey_with_redraw 0x266c calls once per pass,
// each pass spending delay(1) = one 55 ms BIOS tick while no key is down, and
// only when g_location < 0x21 or > 0x7f (never in a dungeon). [0x5884] is
// re-armed to the 12-hour clock by advance_clock (0x5164-0x5183).
//
// Pure: no GameState, no clock, no RNG.
namespace openu5 {

/** 0x41c1-0x41ed: 1 clock (tile & 0xfe) == 0xfa, 2 waterfall (& 0xfc) == 0xd4, 3 fountain (& 0xfc) == 0xd8. */
uint8_t ambient_tile_class(int32_t tile);

/** The 11 x 11 window the scan reads, row-major, the party at (5,5); -1 = off the map. */
struct AmbientWindow {
    static constexpr int kSide = 11;
    int16_t tiles[kSide * kSide];
};
/** 0x4135-0x4247: the class of the nearest sounding object, 0 = none (ties: first in scan order). */
uint8_t ambient_nearest_class(const AmbientWindow &);

/** 0x5164-0x5183: the hour on a 12-hour dial, 0 -> 12. */
uint8_t ambient_chime_hour(uint8_t hour);

/** [0x6a34] (phase) and [0x5884] (chimes left). */
class AmbientTicker {
  public:
    /** A new world (load, title, New Journey): the redraw counters start over. */
    void reset() { phase_ = 0, chimes_ = 0; }
    /** advance_clock ran: [0x5884] = the 12-hour hour. */
    void rearm(uint8_t hour) { chimes_ = ambient_chime_hour(hour); }
    /** One 0x4102 call for the nearest class; the cue it plays (None = silent pass). */
    SfxId tick(uint8_t nearest_class);
    uint8_t phase() const { return phase_; }
    uint8_t chimes() const { return chimes_; }

  private:
    uint8_t phase_ = 0, chimes_ = 0;
};

} // namespace openu5
