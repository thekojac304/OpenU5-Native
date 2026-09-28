#pragma once

#include <cstdint>

#include "movement.h"

// Batch 51 -- the ORIGINAL's scripted-scene timing primitives, named, and what
// each one costs on the device.
//
// Every staged scene in ULTIMA.EXE and its overlays paces itself with exactly
// four kernel primitives. Their bodies were read from the shipped binary for
// this batch (re/tools/dis16.py --exe); the classification letters are the
// batch's own evidence scale:
//
//   A  exact duration proven from the original bytes
//   B  primitive proven, real duration depends on the host machine
//   C  ordering proven, duration unresolved
//   D  native-only compromise
//
//   delay(n)          kernel 0x20fa -- hooks INT 1Ch (int 21h/25h @0x2133),
//                     its handler 0x2159 counts BIOS ticks into [0x5448] and
//                     0x2138 spins until n have elapsed. The binary never
//                     reprograms PIT channel 0, so one tick is 65536/1193182 s
//                     = 54.9254 ms on EVERY machine.                        [A]
//   run_n_frames(n)   kernel 0x3ae6 -- n x (compositor 0x5910 + delay(1)).  [A]
//                     (delay(1) is skipped when the boot calibration is
//                     <= 0xf0, i.e. on a slow machine the frame is render-bound;
//                     the device models the fast-machine tick.)
//   tone_sweep(...)   kernel 0x2192 -- a busy-wait bit-banging the speaker.
//                     It runs the SAME loop with sound OFF (the mute branch
//                     0x21c4 skips only the port 0x61 writes), so it blocks
//                     whether or not a sound is heard. Its outer loop runs a2
//                     "samples"; each sample spins an inner delay of
//                     [0x5356]/24 decrements, where [0x5356] = raw*18/750 and
//                     raw is the boot calibration loop's count per tick
//                     (0x11b4-0x120b) -- i.e. a sample is DESIGNED as 1/1000
//                     of a tick. The outer loop body is not calibrated, so the
//                     real duration is host-dependent.                      [B]
//   fx_tile_fizzle_in kernel 0x1068 -- an LFSR reveal of ONE tile: 256 blits
//                     and 31 compositor calls with NO timer at all. Its
//                     duration is render-bound.                             [C]
//
// A fifth, getkey_with_redraw (0x266c), is an acknowledgement, not a timer;
// the scenes keep it as their own key-wait events and nothing here times it.
namespace openu5 {

/** One INT 1Ch tick (54.9254 ms), rounded as the port has always rounded it:
 *  the same unit as kSceneFrameUnitMs, the Blackthorn pacer's unit and the
 *  tile-animation tick. [A] */
constexpr uint32_t kSceneTickMs = 55;

/** delay(n), kernel 0x20fa: n BIOS ticks. [A] */
constexpr uint32_t delay_ticks_ms(uint32_t n) { return n * kSceneTickMs; }
/** run_n_frames(n), kernel 0x3ae6: n x (render + delay(1)). [A] */
constexpr uint32_t run_n_frames_ms(uint32_t n) { return n * kSceneTickMs; }

/**
 * tone_sweep's sample rate as the device holds it. [B]
 *
 * This is the project's one established calibration of the sweep's inner
 * delay, not a new number: game/src/skin/fiel/speaker.ts measures
 * DELAY_UNIT_MS = C*t_dec = 0.93 ms from DOSBox-X captures of the original
 * (the ultima_001.wav cascade) and derives SPEAKER_SAMPLE_RATE_HZ = 24000/0.93
 * = 25806; AlphaRuntime::start_magic_ceremony already uses the same figure.
 * Against the bytes' 1/1000-tick design it corresponds to a dec-loop cost of
 * 0.706 of the calibration loop, and it is the inner-delay FLOOR: the
 * uncalibrated outer loop only ever adds to it. Two independent witnesses of
 * the apparition's chord bracket it from above -- ~2.75 s (the capture
 * skin/fiel/apparition.ts was calibrated on) and 4.57 s (Lord Fenton corpus,
 * re/notes/ceremonias-cadencia-medida.md section 1) -- so a device hold of
 * tone_sweep_ms() is never LONGER than any 1988 host held it.
 */
constexpr uint32_t kToneSweepSamplesPerSecond = 25806;
/** How long tone_sweep holds the screen for `samples` outer iterations. [B] */
constexpr uint32_t tone_sweep_ms(uint32_t samples) {
    return uint32_t((uint64_t(samples) * 1000u) / kToneSweepSamplesPerSecond);
}

/**
 * fx_tile_fizzle_in has no timer (C), so the device cannot recover its length.
 * What it CAN guarantee is that the tile the fizzle starts from reaches the
 * screen as its own frame instead of being replaced inside the same pump: one
 * presentation tick, the minimum any intermediate scene state is held. [D]
 */
constexpr uint32_t kFizzleFloorMs = kSceneTickMs;

// ---------------------------------------------------------------------------
// Camp apparition -- OUTSUBS.OVL camp_results 0x0658 (near calls rebased by
// 0xA290; argument order is C order a0..a4 = [bp+4]..[bp+0xc], a2 the sample
// count, a3 = 1 at every site).
// ---------------------------------------------------------------------------

/** The cue ids rest.cpp emits at each of the scene's tone_sweep calls. */
constexpr char kApparitionMaterializeCue[] = "apparition-materialize"; // 0x067b
constexpr char kApparitionArpeggioCue[] = "apparition-arpeggio";       // 0x0686-0x06a2
constexpr char kApparitionChimeCue[] = "apparition-heal-chime";        // 0x0896
constexpr char kApparitionChordCue[] = "apparition-chord";             // 0x08c1

/** 0x066f `mov ax,0x2710`: the materialization sweep. */
constexpr uint32_t kApparitionMaterializeSamples = 0x2710;
/** 0x0683-0x06a2: six sweeps (si = 0x3a26 .. <0x3a32 step 2), each a2 = 0x1388. */
constexpr uint32_t kApparitionArpeggioNotes = (0x3a32 - 0x3a26) / 2;
constexpr uint32_t kApparitionArpeggioSamples = kApparitionArpeggioNotes * 0x1388;
/** 0x088a `mov ax,0x1388`: the per-member heal chime, before the XOR. */
constexpr uint32_t kApparitionChimeSamples = 0x1388;
/** 0x08b5 `mov ax,0xea60`: the per-member chord that freezes the XOR frame. */
constexpr uint32_t kApparitionChordSamples = 0xea60;
/** 0x087b-0x087f: run_n_frames(1) renders the member standing. */
constexpr uint32_t kApparitionWakeFrames = 1;
/** 0x08c4-0x08d9: three run_n_frames(1); the first one repaints over the XOR. */
constexpr uint32_t kApparitionRestoreFrames = 3;

/** True for the events that make up the apparition's staged stream. */
bool camp_apparition_event(const GameEvent &);
/**
 * The blocking wait the original performs AFTER the point this event marks,
 * in device milliseconds; 0 when the next beat follows at once. PURE.
 */
uint32_t camp_apparition_wait_ms(const GameEvent &);

// ---------------------------------------------------------------------------
// Blackthorn capture -- BLCKTHRN.OVL (near calls rebased by 0xA290).
// ---------------------------------------------------------------------------

/** 0x0833 `mov ax,0x32c8`: the materialization sweep, BEFORE slot 8 exists. */
constexpr uint32_t kBlackthornMaterializeSamples = 0x32c8;
/**
 * sacrifice_member's siren, 0x03d0-0x040f: si runs 0x7d0 -> <0x61a8 step 0x32
 * and back 0x61a8 -> >0x7d0 step 0x32 -- 460 sweeps each way -- and every
 * sweep is a2 = 0xc8 (0x03db / 0x03fd). The same bounds and stride as the
 * shard ritual (CAST 0x15dd-0x162a), which is why the reference reuses its cue.
 */
constexpr uint32_t kBlackthornSirenSweepsPerLeg = (0x61a8 - 0x7d0) / 0x32;
constexpr uint32_t kBlackthornSirenSamples = 2 * kBlackthornSirenSweepsPerLeg * 0xc8;
/**
 * A3-HF8 (H-186). The sacrifice burst, 0x0414-0x041e -> explosion_fx_at_cell
 * (ULTIMA.EXE 0x3522): blit_tile(cell, 0) @0x354b, noise_burst(0x7d0, 0xbb8,
 * 0xa) @0x355a, viewport_redraw @0x355d. The tile stays up for exactly the
 * noise burst the kernel blocks in: ceil(0xbb8 / 0xa) draws of 0xa x (C >> 4),
 * each unit 1.5 speaker samples (sfx_synth.h kNoiseUnitHalfSamples = 3 half
 * samples) -- 4,500 samples at tone_sweep's rate, 174 ms, the same length as
 * the combat hit cue's 0x35de burst with these very arguments. [B]
 */
constexpr uint32_t kBlackthornBurstSamples = ((0xbb8 + 0xa - 1) / 0xa) * 0xa * 3 / 2;
static_assert(tone_sweep_ms(kBlackthornBurstSamples) == 174, "9,000 half samples at 2 x 25,806 Hz");

} // namespace openu5
