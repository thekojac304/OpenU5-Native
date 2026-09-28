#pragma once

#include <cstdint>

#include "movement.h"
#include "presentation.h"
#include "scene_timing.h"

// A3-HF7 (H-184 / D-41) -- the shrine rite's viewport negative and the Codex
// ceremony's three XOR pulses, as the device presents them.
//
// Read from the 1988 binary for this batch (re/tools/dis16.py; CAST2.OVL's
// near calls resolve through base 0xE1E0: 0x2890 -> 0x0a70 set_color,
// 0x29a6 -> 0x0b86 rect with `stc` = the EGA driver's XOR write mode,
// 0x3fb2 -> 0x2192 tone_sweep, 0x4e92 -> 0x3072 screen_shake_fx,
// 0x448c -> 0x266c getkey_with_redraw, 0x5906 -> 0x3ae6 run_n_frames):
//
//   donation ("ALAKAZAM!")  0x0bbc set_color([0x13b0]); 0x0bcd rect(8,8,0xb7,0xb7) XOR
//                           0x0bd0-0x0c0f two tone_sweep loops, 460 calls each, a2 = 0xc8
//                           0x0c14 jmp 0x0d16 -> 0x0d1a run_n_frames(10)
//   WELL DONE               0x0c29 print; 0x0c34 set_color([0x13b0]); 0x0c41 rect XOR
//                           0x0c44-0x0c83 two tone_sweep loops, 460 calls each, a2 = 0x96
//                           0x0c88 screen_shake_fx; 0x0c8b-0x0d11 karma, STR/DEX/INT
//                           (+ their prints); 0x0d1a run_n_frames(10)
//   Codex ceremony          after the page's getkey 0x0d9f, if all eight shrines:
//                           0x0dac set_color([0x13ae]) 0x0dbd rect XOR, 0x0dc0 shake
//                           0x0dc3 set_color([0x13b0]) 0x0dd4 rect XOR, 0x0dd7 shake
//                           0x0dda set_color([0x13ae]) 0x0deb rect XOR, 0x0dee shake
//                           0x0df1 print "A STRANGE WIND...", 0x0df8 getkey
//
// The rect (8,8)-(0xb7,0xb7) is the 176x176 map viewport exactly: the panels
// and the text window never invert. Nothing un-XORs the rect: what restores
// the viewport is the next REDRAW (compositor 0x5910) -- the first frame of
// run_n_frames(10) for the donation and WELL DONE, the getkey's first idle
// pass at 0x0df8 (0x1b38 polls, delay(1), then 0x269a redraws; the Codex is
// on the surface, location 0 < 0x21) for the Codex. screen_shake_fx moves the
// viewport's bands on the screen itself (0x71ca / 0x7200 / 0x0ace) and redraws
// nothing, so the Codex's three XORs ACCUMULATE: 4, 4^15 = 11, 11^4 = 15.
// The colour registers are INTRO.OVL 0x09f4 `mov [0x13ae],4` and 0x09fa
// `mov [0x13b0],0xf` (the EGA/Tandy branch; set_color masks with 0xF).
//
// Nothing here reads a key: tone_sweep, screen_shake_fx and run_n_frames are
// busy loops, so the whole effect blocks the caller. The core has already
// committed every rule (karma, attributes, the quest bits) when the events
// are emitted; this model decides only what the viewport shows and how long
// the presentation holds after each event.
namespace openu5 {

/** [0x13b0] after INTRO.OVL 0x09fa: the rite's single, loose XOR. [A] */
constexpr uint8_t kRitualInvertMask = 0x0f;
/** The Codex's three XORs: [0x13ae], [0x13b0], [0x13ae] (INTRO 0x09f4/0x09fa). [A]
 *  The core carries each as the `note` of the ceremony's Quake it brackets. */
constexpr uint8_t kCodexPulseMasks[3] = {0x04, 0x0f, 0x04};
/** 0x0bd0/0x0c44: si = 0x7d0 .. <0x61a8 step 0x32, and back: 460 calls a loop. [A] */
constexpr uint32_t kRitualSweepCalls = (0x61a8 - 0x7d0) / 0x32;
/** WELL DONE: two loops of tone_sweep(0xc1c, 1, 0x96, si, 0). [A]; ms [B] */
constexpr uint32_t kWellDoneSweepSamples = 2 * kRitualSweepCalls * 0x96;
/** Donation: two loops of tone_sweep(0xa8c, 1, 0xc8, si, 0). [A]; ms [B] */
constexpr uint32_t kDonationSweepSamples = 2 * kRitualSweepCalls * 0xc8;
/** 0x0d16 `mov ax,0xa` / 0x0d1a run_n_frames: its first frame restores. [A] */
constexpr uint32_t kRitualRestoreFrames = 10;
/** 0x266c's idle pass: 0x1b38's delay(1) before the 0x269a redraw. [A] */
constexpr uint32_t kGetkeyRedrawFrames = 1;

constexpr char kWellDoneCue[] = "shrine-well-done";
constexpr char kDonationCue[] = "shrine-donation";

/** A Quake the core tagged as one of the Codex's bracketed shakes. */
constexpr bool codex_pulse_quake(const GameEvent &e) {
    return e.kind == GameEventKind::Quake && e.note > 0 && e.note <= 0x0f;
}

class RitualFx {
  public:
    /**
     * Present `e`: apply what it does to the viewport. Call once per presented
     * event, in order. Returns true when the viewport's mask changed.
     */
    bool present(const GameEvent &e);
    /**
     * How long the original blocks AFTER `e`, read on the state BEFORE `e` is
     * presented (the pacer asks, then delivers). 0 = the next event follows.
     */
    uint32_t hold_after_ms(const GameEvent &e) const;
    /** The XOR mask the viewport shows now (0 = the normal picture). */
    uint8_t mask() const { return mask_; }
    /** A rite or ceremony effect has not been restored yet. */
    bool active() const { return mask_ != 0 || loose_ || pulses_ != 0; }
    uint8_t pulses() const { return pulses_; }
    /** A load, a new journey: nothing stays inverted. */
    void clear() { mask_ = 0; loose_ = false; pulses_ = 0; }

  private:
    uint8_t mask_ = 0;
    bool loose_ = false; // donation / WELL DONE: restored by run_n_frames(10)
    uint8_t pulses_ = 0; // Codex XORs presented since the last restore
};

} // namespace openu5
