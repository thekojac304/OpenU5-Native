#pragma once

#include <cstddef>
#include <cstdint>

// Y-04 (PoisonTick) -- the WALKING POISON TICK's feedback, and the roster-row
// inversion primitive it is built on. Native mirror of
// game/src/ui/poison-tick.ts and the precedence rule in
// game/src/skin/fiel/roster.ts.
//
// DERIVATION (ULTIMA.EXE, bodies read). `kernel_turn_housekeeping` 0x2AE8
// walks the party slots in ASCENDING order (loop 0x2b0b, `di` = index) and for
// every member whose status is 'P' (0x2b36 `cmp ax,0x50`) calls
// `kernel_apply_damage(i,1)` (0x2b3b/0x2b3c-0x2b3f/0x2b40 `call 0x2a52`).
// 0x2a52 is both the damage AND its presentation, IN THIS ORDER:
//
//     2a56: push [bp+6]        ; the slot
//     2a59: call 0x2a28        ; INVERTS that slot's roster row
//     2a5c: push 0xa           ; step = 10
//     2a5f: push 0x640         ; dur  = 1600
//     2a64: push 0x7d0         ; band = 2000
//     2a68: call 0x223c        ; noise_burst -- the sound of the hit
//     2a6b: push [bp+6]
//     2a6e: call 0x2a28        ; UN-inverts (0x2a28 is an XOR; the 2nd call restores)
//     2a7b: sub [si+0x55b8],ax ; and ONLY THEN subtracts the HP
//
// 0x2a28 is the SHARED row-inversion routine: the `select_party_member` picker
// cursor (0x2daf/0x2dc4/0x2e1f/0x2e7d) and the combat hit (0x35ba/0x35cd) use
// the same one. This pacer therefore paints nothing of its own -- it publishes
// a row index and the renderer routes it to the same inversion as always.
//
// SEQUENCING. 0x223c is a calibrated wait loop with the speaker gate open; the
// original is single-threaded and BLOCKS until it finishes. With N poisoned
// members that is N COMPLETE, CONSECUTIVE sequences in SLOT order, not N
// overlapping sounds.
//
// STATE-ORDER DIVERGENCE, DECLARED. In the binary the HP subtraction happens
// AFTER the flash (0x2a7b follows 0x2a6e). In native, as in the TypeScript
// reference, `turn.cpp` has already applied the poison damage synchronously by
// the time the `PoisonTick` event is emitted, and this pacer only presents
// what already happened. The port makes the same trade the reference makes,
// for the same reason: deferring a committed HP write to a wall-clock
// presentation would put a save, a reload or a non-interactive consumer out of
// sync with the rules. Nothing the player observes changes -- the flash still
// runs slot by slot, in slot order, at the reference cadence.
//
// NOT MODAL, on purpose: the reference pacer swallows no input and defers no
// part of the turn, because the tick happens on EVERY step and blocking the
// walk would be worse than the defect it fixes.
namespace openu5 {

/**
 * Duration of the `combat-damage` cue = `noise_burst(step=10, dur=1600,
 * band=2000)` (kernel 0x2a52 @0x2a68), in ms, by the SAME model the reference
 * synthesizer uses: `samplesToMs(dur * 1.5, 1)` with a 24000/0.93 Hz speaker
 * sample rate -- 1600 * 1.5 * 1000 / (24000/0.93) = 93 ms exactly. It is the
 * spacing between poisoned members (the original chains them by blocking).
 * Not an eyeballed calibration: it is the number the synthesizer programs.
 */
constexpr uint32_t kPoisonBlipMs = 93;

/** The binary's roster is six rows. */
constexpr size_t kPoisonMaxSlots = 6;

/**
 * Plays the script: for each slot IN ORDER, invert its row and sound the
 * noise; when the blips run out, switch the inversion off. Timer/tick driven,
 * never blocking. With `blip_ms == 0` every cue is emitted at once and no
 * flash is left on -- the drain-synchronously mode host harnesses use, exactly
 * as the reference does under automation.
 */
class PoisonFlashPacer {
  public:
    void set_blip_ms(uint32_t ms) { blip_ms_ = ms; }
    uint32_t blip_ms() const { return blip_ms_; }

    /** Start the script. Releases the first blip at `now_ms` itself. */
    void run(const uint8_t *slots, uint8_t count, uint32_t now_ms);

    /** Release whatever is due. Returns true when anything changed. */
    bool pump(uint32_t now_ms);

    /** Cut the sequence and switch the flash off (skin change, save load). */
    void cancel();

    /** Roster row currently inverted, or -1 when none is. */
    int8_t flash_row() const { return flash_; }
    bool active() const { return active_; }
    /** Damage cues emitted so far -- the owner turns these into its SFX. */
    uint32_t cues() const { return cues_; }
    /** Blips still to come, including the one on screen. */
    uint8_t remaining() const { return active_ ? uint8_t(count_ - index_) : 0; }

  private:
    uint8_t slots_[kPoisonMaxSlots]{};
    uint8_t count_ = 0, index_ = 0;
    int8_t flash_ = -1;
    bool active_ = false;
    uint32_t blip_ms_ = kPoisonBlipMs, next_at_ = 0, cues_ = 0;
};

/**
 * WHICH roster row goes in REVERSE VIDEO, and in what precedence. All three
 * markers are the SAME primitive of the binary (0x2a28, the inversion of the
 * row's rectangle), so they cannot share a row and an order is needed:
 *
 *   1. `damage_flash` -- `kernel_apply_damage` 0x2a52 (@0x2a59/0x2a6e). ALWAYS
 *      wins while it lasts, because 0x2a28 is an XOR over the framebuffer: it
 *      paints over whatever was there.
 *   2. `ztats_cursor` -- the Ztats modal candidate (same picker 0x2d7a).
 *   3. `select_cursor` -- the picker cursor (the Camp guard prompt).
 *   4. `combat_actor` -- the actor whose turn it is (`g_cmb_actor`).
 *
 * -1 in every argument = no inverted row.
 */
constexpr int8_t roster_invert_row(int8_t damage_flash, int8_t ztats_cursor,
                                   int8_t select_cursor, int8_t combat_actor) {
    return damage_flash >= 0   ? damage_flash
           : ztats_cursor >= 0 ? ztats_cursor
           : select_cursor >= 0 ? select_cursor
                                : combat_actor;
}

} // namespace openu5
