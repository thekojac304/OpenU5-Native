#pragma once

#include <cstddef>
#include <cstdint>

#include "combat.h"
#include "sfx_synth.h"

// D-63 (A3-HF3) -- the arena's HIT CUE: `kernel_combat_hit_flash`, ULTIMA.EXE
// 0x3564, read for this batch (ALPHA3_AUDIO.md section 27):
//
//     359f: call 0x10e0        ; blit_tile(cell of the target, tile 0) -- the
//                              ;   marker. EGA.DRV sel 0x51 (fn27, 0x1637)
//                              ;   writes whole plane bytes: OPAQUE, the cell
//                              ;   shows tile 0 (TileData `Explosion`, the
//                              ;   8-point star) instead of the combatant.
//     35ac: test [bx+2],0x80   ; is the target a PARTY member?
//     35ba: call 0x2a28        ;   yes: XOR its roster row (slot [bx+3])
//     35c9: call 0x223c        ;        noise_burst(0x1f4, 0xbb8, 0x28)
//     35cd: call 0x2a28        ;        XOR it back (0x2a28 is involutive)
//     35de: call 0x223c        ;   no:  noise_burst(0x7d0, 0xbb8, 0xa)
//     35e1: call 0x5910        ; viewport_redraw -- the marker is gone
//
// The CALLER then applies the strike (COMSUBS 0x0bf8: 0x0c30 the cue, 0x0c39
// 0x194A the damage / death / status, 0x0c42 0x0312 the " hit!" line). So the
// cue FOLLOWS a decided hit and PRECEDES its outcome, and it is the same for a
// graze, a kill, a poisoning or a sleep: all of them happen inside 0x194A.
// Its other callers: COMSUBS 0x0b99 (ranged), COMBAT 0x0200 / 0x03c1 (enemy
// ranged / melee; food theft skips it), 0x1c17 / 0x1c5a (fire / poison field),
// 0x0c12, and CAST 0x0963, 0x20ad, 0x20d2, 0x20dd, 0x2113.
//
// DURATION. 0x223c is a calibrated busy loop; the original is single-threaded
// and blocks in it, so the row stays inverted and the marker stays up for
// exactly the burst. Both bursts program ceil(0xbb8 / step) iterations of
// `step` units, 9,000 half sweep samples, which is 174.4 ms on the speaker
// model every SFX here is rendered with (kSpeakerSweepRate). Not a tuned
// animation length: it is the length of the sound the device already plays.
//
// SEQUENCING. Because the original blocks, N hits are N complete cues one
// after another, each followed by the restore (0x2a28, 0x5910) and the rest of
// the turn before the next can start. Native presents a whole command's events
// at once, so the cues queue here in event order and play one at a time,
// never overlapping and never dropped below the queue's capacity. The pause
// between two queued cues is NOT derivable: in the original it is whatever the
// repaint, the strike, the message and the next attacker cost. Native uses one
// presentation unit (55 ms, one INT 1Ch tick) -- a declared native choice,
// just long enough that the same row or cell hit twice shows two cues.
//
// STATE ORDER, DECLARED (as for the poison blip, poison_tick.h): the core has
// already committed the strike when the event is emitted, so the inverted row
// shows the NEW hit points, and a killed combatant's cell shows its corpse once
// the marker is gone. What the player sees in order is unchanged: the cue,
// then the result.
//
// Presentation only: nothing here reads or writes GameState or any RNG.
namespace openu5 {

/** noise_burst 0x223c's length in half sweep samples: ceil(dur / step) draws of step x (C >> 4). */
constexpr uint32_t noise_burst_half_samples(uint32_t step, uint32_t dur) {
    const uint32_t st = step ? step : 1;
    const uint32_t n = (dur + st - 1) / st;
    return (n ? n : 1) * st * kNoiseUnitHalfSamples;
}
/** 0x35c9, a party member hit: noise_burst(band 0x1f4, dur 0xbb8, step 0x28). */
constexpr uint32_t kCombatHitCueHalfSamples = noise_burst_half_samples(0x28, 0xbb8);
static_assert(kCombatHitCueHalfSamples == noise_burst_half_samples(0xa, 0xbb8),
              "0x35de (an enemy hit) lasts exactly as long as 0x35c9");
/** The cue's length: the burst's, rounded to the millisecond (174.38 ms). */
constexpr uint32_t kCombatHitCueMs =
    (kCombatHitCueHalfSamples * 1000u + kSpeakerHalfSampleRate / 2) / kSpeakerHalfSampleRate;
static_assert(kCombatHitCueMs == 174, "9,000 half samples at 2 x 25,806 Hz");
/** Restore time between two queued cues: one presentation unit (native choice, see header). */
constexpr uint32_t kCombatHitCueGapMs = 55;
/** blit_tile's id at 0x359f: tile 0, TileData `Explosion` (the 8-point star). */
constexpr int16_t kCombatHitMarkerTile = 0;
/** Cues that can wait. A command's hits are one per struck combatant at most, except
 *  the triple strike; 32 covers the arena's 22 actors and a mass spell's overflow. */
constexpr size_t kCombatHitCueSlots = 32;

/**
 * The strikes the core reports as a MESSAGE, not as `Attacked`: a poisoning of a
 * healthy member (COMBAT 0x18c9) and a sleep strike (0x19ab, In Zu). Both happen
 * inside 0x194A, after the caller's 0x3564, so the original cues them like any hit.
 */
bool combat_status_hit(const CombatEvent &);
/** The combatant a 0x3564 cue falls on for this event, or -1 when the event is not one. */
int32_t combat_hit_cue_target(const CombatEvent &);

struct CombatHitCue {
    int8_t x = -1, y = -1;   // the target's arena cell
    int8_t member = -1;      // its roster slot, -1 for an enemy (0x35ac)
};

/**
 * Plays the cues of a command, one at a time, from the frame clock. Never blocks.
 * The owner turns `marker()` into a tile-0 blit on the arena cell and
 * `flash_row()` into the roster row's reverse video (the shared 0x2a28 channel).
 */
class CombatHitCuePacer {
  public:
    /** Queue a cue. It starts at `now_ms` itself when nothing is showing or waiting. */
    bool push(const CombatHitCue &, uint32_t now_ms);
    /** Advance to `now_ms`. Returns true when what is on screen changed. */
    bool pump(uint32_t now_ms);
    /** Drop everything (a load, leaving the arena by another path). */
    void cancel();

    /** A cue is showing or waiting: the frame clock must keep running. */
    bool active() const { return showing_ || count_ != 0; }
    /** Roster slot in reverse video, or -1. */
    int8_t flash_row() const { return showing_ ? on_.member : int8_t(-1); }
    /** The cell under the marker, when one is up. */
    bool marker(int8_t &x, int8_t &y) const {
        if (!showing_) return false;
        x = on_.x;
        y = on_.y;
        return true;
    }
    size_t queued() const { return count_; }
    uint32_t shown() const { return shown_; }
    uint32_t dropped() const { return dropped_; }

  private:
    CombatHitCue queue_[kCombatHitCueSlots]{};
    CombatHitCue on_{};
    uint8_t head_ = 0, count_ = 0;
    bool showing_ = false, restoring_ = false;
    uint32_t ends_at_ = 0, ready_at_ = 0;
    uint32_t shown_ = 0, dropped_ = 0;
};

} // namespace openu5
