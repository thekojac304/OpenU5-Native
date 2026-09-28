#pragma once

#include <cstddef>
#include <cstdint>

#include "openu5/audio.h"

// Alpha 3 A3-02 -- the PC-speaker synthesizer. Derivations and the cue table:
// native/targets/tdeck/ALPHA3_AUDIO.md section 15.
//
// Three layers, all pure (no hardware, no clock, no GameState, no g_rng):
//   1. compile_sfx(): one SfxId (+ its parameter) -> a SpeakerProgram, the
//      original's own sequence of speaker primitive calls with the arguments
//      the binary pushes (re/tools/a3_02_cue_sites.py prints every site);
//   2. SpeakerVoice: renders a program to 16-bit PCM by EMULATING each
//      primitive's loop (ULTIMA.EXE 0x2192 / 0x223c / 0x22c0 / 0x43ae / 0x20c8)
//      at the project's one calibration, box-filtered from a 4x internal rate;
//   3. SfxPlayer: one monophonic voice (the speaker was one gate) plus a short
//      pending FIFO, with the A3-02 priority policy. The device's audio task
//      owns one; the host tests drive another directly.
//
// Audio never drives timing: nothing here is ever waited on by the game, and
// a program's length only matches a paced scene hold because both are derived
// from the same constant (scene_timing.h kToneSweepSamplesPerSecond).
namespace openu5 {

// ---------------------------------------------------------------------------
// Calibration and output format.
// ---------------------------------------------------------------------------
/**
 * tone_sweep iterations per second with a3 = 1: the project's one calibration
 * (speaker.ts DELAY_UNIT_MS = 0.93 ms from DOSBox-X captures; scene_timing.h).
 * It is used as the integer 25806, the pacers' figure, rather than the
 * reference's 24000/0.93 = 25806.45: 17 ppm, and the price of rendered sweeps
 * matching the paced holds frame for frame.
 */
constexpr uint32_t kSpeakerSweepRate = 25806;
/** Durations are counted in HALF sweep samples: noise_burst's C>>4 unit is 1.5. */
constexpr uint32_t kSpeakerHalfSampleRate = 2 * kSpeakerSweepRate;
/** delay 0x20c8 with shift index 1 (table [0x5427] = 0): inner = C = 24 samples. */
constexpr uint32_t kDelayUnitHalfSamples = 48;
/** noise_burst 0x223c: each iteration spins `step` x (C >> 4) = 1.5 samples. */
constexpr uint32_t kNoiseUnitHalfSamples = 3;
/** The 8253's input clock, the dividend of 0x227b/0x22f2 (`dx:ax = 0x1234DE`). */
constexpr uint32_t kPitClockHz = 0x1234de;
/** The device's I2S rate (A3-01, hardware-validated). */
constexpr uint32_t kSfxOutputRateHz = 16000;
/** Internal oversampling of the gate before the box filter. */
constexpr uint32_t kSfxOversample = 4;
/** Peak of a 50 % square before the SFX gain: -12 dBFS, the A3-01 test tone's level. */
constexpr int32_t kSpeakerAmplitude = 8192;
/** [0x545c]'s initial value in the DS image (DATA.OVL fileoff 0x546c). */
constexpr uint16_t kNoiseSeed = 0x7664;

/** The frequency the speaker really emits for a pushed value v: clock / floor(clock / v). */
uint32_t pit_quantized_millihertz(uint32_t value);
/** tone_sweep's fundamental for `inc`: inc / 65536 x 25806 Hz, in mHz. */
uint32_t sweep_millihertz(uint16_t inc);
/** The noise PRNG step at 0x2255-0x2262: ((s + 0x9248) ror 3 ^ 0x9248) + 0x11. */
uint16_t speaker_noise_next(uint16_t state);
/** One noise_burst draw (0x2255-0x2277): advance `state`, return 100 + s % (band - 99). */
uint16_t speaker_noise_draw(uint16_t &state, uint16_t band);

// ---------------------------------------------------------------------------
// Programs.
// ---------------------------------------------------------------------------
enum class SpeakerPrimitive : uint8_t {
    None,
    Tone,    // beep 0x22c0 = set_tone 0x22e2 + delay 0x20c8 + stop 0x230e: one PIT square
    Glide,   // glide 0x43ae: a staircase of set_tone writes, one per delay(step)
    Sweep,   // tone_sweep 0x2192: the 1-bit PWM loop (pitch from inc, duty from bx)
    Noise,   // noise_burst 0x223c: a PIT square re-pitched by the local PRNG each iteration
    Silence, // delay 0x20c8 with the speaker gate closed
    // A3-03. screen_shake_rumble 0x3072: the gate stays open while the loop
    // writes a new PIT count set_tone(rand_range(0x13, 0x96)) at every step
    // of the shake. set_tone writes the count only (0x22f2-0x22fe, no control
    // word), so in mode 3 each write takes effect at the next half-cycle: the
    // speaker plays square half-cycles of independently drawn low pitches.
    Rumble
};
const char *speaker_primitive_name(SpeakerPrimitive);

/**
 * One primitive call. `iterations` x `iteration_half_samples` is its length;
 * the rest is the primitive's own arguments, as the binary holds them:
 *   Tone    value = the pushed frequency
 *   Glide   value = start (si), delta = the derived per-step increment
 *   Sweep   value = inc (dx step), start = bx, delta = bx step
 *   Noise   value = band (the draw is 100 + s % (band - 99))
 *   Rumble  value = the lowest draw, start = the highest (rand_range bounds)
 *
 * A3-03. A Sweep segment may stand for a LOOP of `calls` identical
 * tone_sweep calls whose only changing argument is the bx start, which the
 * loop moves by `call_stride` between calls (the shard / shrine / Blackthorn
 * "two 460-call loops"): each call restarts dx = 0, bx = start + k x stride,
 * and runs `iterations / calls` iterations. calls = 1 is the plain call.
 */
struct SpeakerSegment {
    SpeakerPrimitive kind = SpeakerPrimitive::None;
    uint16_t value = 0;
    uint16_t start = 0;
    int16_t delta = 0;
    uint32_t iterations = 0;
    uint32_t iteration_half_samples = 0;
    uint16_t calls = 1;
    int16_t call_stride = 0;
};

constexpr size_t kMaxSpeakerSegments = 24;
struct SpeakerProgram {
    SpeakerSegment segments[kMaxSpeakerSegments]{};
    uint8_t count = 0;
    /** Sum of every segment's half samples (the program's length). */
    uint64_t half_samples() const;
};

// The primitive constructors, with the binary's C argument order documented
// against the push order the reference writes (push order = reverse of C).
/** beep 0x22c0(dur, freq): `freq` for dur delay units. */
SpeakerSegment speaker_beep(uint16_t freq, uint16_t dur);
/** glide 0x43ae(total, step, end, start). inc = trunc16((end-start)*step) / total. */
SpeakerSegment speaker_glide(uint16_t start, uint16_t end, uint16_t step, int16_t total);
/** tone_sweep 0x2192(step, start, count, delay, inc). */
SpeakerSegment speaker_sweep(uint16_t inc, uint16_t delay, uint16_t count, uint16_t start, int16_t step);
/** noise_burst 0x223c(band, dur, step): ceil(dur / step) iterations. */
SpeakerSegment speaker_noise(uint16_t step, uint16_t dur, uint16_t band);
/** delay 0x20c8(count, 1) with the gate closed. */
SpeakerSegment speaker_silence(uint16_t count);
/** A device-only tone of a fixed length (the Developer test tone; class D). */
SpeakerSegment speaker_fixed_tone(uint16_t hz, uint32_t milliseconds);
/**
 * A3-03. A loop of `calls` tone_sweep(inc, delay, count, start + k x stride, step)
 * calls, k = 0 .. calls - 1 (see SpeakerSegment).
 */
SpeakerSegment speaker_sweep_loop(uint16_t inc, uint16_t delay, uint16_t count, uint16_t start, int16_t step,
                                  uint16_t calls, int16_t stride);
/**
 * A3-03. screen_shake_rumble 0x3072's sound: set_tone(rand_range(lo, hi)) for
 * `milliseconds`. The loop has no timer of its own (its pace is the re-blit
 * of the viewport), so the length is the shake's measured window.
 */
SpeakerSegment speaker_rumble(uint16_t lo, uint16_t hi, uint32_t milliseconds);
/** 0x3072's shake: 8 cycles, one visible pulse each (presentation.h kQuakePeriodMs). */
constexpr uint32_t kRumbleCycles = 8;
constexpr uint32_t kRumbleCycleMs = 117;
/** The draw at 0x30ac-0x30b4: rand_range(0x13, 0x96) -- 19 .. 150 (Hz, set_tone's unit). */
constexpr uint16_t kRumbleLowest = 0x13, kRumbleHighest = 0x96;
/**
 * The rumble's own draw word. The original draws from the game RNG [0x5420]
 * inside the render path -- a registered deliberate divergence
 * (re/notes/oracle-flash-rng.md): the port never consumes g_rng for
 * presentation, so the SEQUENCE is class C while the law is exact.
 */
constexpr uint16_t kRumbleSeed = 0x5420;
/** rand_range 0x2092's arithmetic on a private word: step, & 0x7fff, lo + v % (hi - lo + 1). */
uint16_t speaker_rumble_draw(uint16_t &state, uint16_t lo, uint16_t hi);
/**
 * A3-03. A hand-written set_tone ramp (TOWN 0x0fb3-0x0fd1): set_tone(v);
 * delay(delay, 1); v += delta -- `steps` times. A Glide segment whose
 * increment is the literal delta rather than glide 0x43ae's derived one.
 */
SpeakerSegment speaker_ramp(uint16_t start, int16_t delta, uint32_t steps, uint16_t delay);

/**
 * The cue table (A3-02, completed in A3-03). true = `program` holds the
 * original's sequence for this cue; false = no sound (a marker cue whose
 * sound belongs to a sibling event, music, a presentation the device does not
 * run, or no adjudicated sound -- sfx_inventory.h says which). PURE.
 */
bool compile_sfx(SfxId, int32_t param, SpeakerProgram &program);
/** compile_sfx(id, 0, ...) would succeed. */
bool sfx_supported(SfxId);
/** Output frames a segment occupies: floor(half samples x 16000 / 51612). */
uint32_t speaker_segment_frames(const SpeakerSegment &);
/** The sum of the program's segment frames (each floored on its own). */
uint32_t speaker_program_frames(const SpeakerProgram &);

// ---------------------------------------------------------------------------
// The renderer. It runs at kSfxOutputRateHz only: the noise path's two
// one-pole filters are fixed-point constants for that rate.
// ---------------------------------------------------------------------------
class SpeakerVoice {
  public:
    /**
     * Start `program` (copied). `noise_state` is [0x545c], shared across calls;
     * `rumble_state` the rumble's draw word (nullptr = a private one).
     */
    void start(const SpeakerProgram &program, uint16_t *noise_state, uint16_t *rumble_state = nullptr);
    /**
     * Render up to `frames` samples, UNSCALED by any volume. Returns how many
     * it produced; fewer than asked (0 included) means the program ended.
     */
    size_t render(int32_t *out, size_t frames);
    /** Fade out over `frames` and end (a cancel or a preemption). */
    void release(uint32_t frames);
    bool active() const { return active_; }
    bool releasing() const { return release_total_ != 0; }
    /** Frames produced since start(). */
    uint32_t position() const { return position_; }

  private:
    bool begin_segment();
    void begin_iteration();
    int32_t fine_level();

    SpeakerProgram program_{};
    uint16_t *noise_ = nullptr;
    uint16_t *rumble_ = nullptr;
    uint16_t own_rumble_ = kRumbleSeed;
    // rumble: fine-rate PIT phase against the current half-cycle
    uint64_t rumble_phase_ = 0, rumble_half_ = 0;
    bool active_ = false;
    uint8_t index_ = 0;
    uint32_t position_ = 0;
    // current segment
    uint32_t frames_ = 0, frame_ = 0, edge_ = 0;
    uint64_t fine_ = 0, next_boundary_ = 0;
    uint32_t iteration_ = 0;
    // PIT square (Tone / Glide / Noise)
    uint32_t phase_ = 0, phase_inc_ = 0;
    uint16_t value_ = 0;
    // tone_sweep PWM
    uint16_t dx_ = 0, bx_ = 0, threshold_ = 0;
    bool gate_ = false;
    // noise filters (unipolar gate -> DC block -> cone low-pass)
    int32_t hp_in_ = 0, hp_out_ = 0, lp_out_ = 0;
    // release
    uint32_t release_total_ = 0, release_left_ = 0;
};

// ---------------------------------------------------------------------------
// Playback policy.
// ---------------------------------------------------------------------------
/**
 * What a cue is, for the preemption rule (ALPHA3_AUDIO.md sections 15.4 and
 * 16.11). Ambient (A3-03) is the lowest: it never waits in the FIFO (a busy
 * voice skips the tick -- the next one comes 55 ms later) and any other cue
 * cuts it.
 */
enum class SfxClass : uint8_t { Ambient, Ordinary, Instrument, Combat, Spell, Scene, Diagnostic };
SfxClass sfx_class(SfxId);
const char *sfx_class_name(SfxClass);
/**
 * A3-03. True only for the cues a HELD KEY repeats (a step, a wall bump): a
 * request identical to the newest pending one adds nothing. Every other
 * repeat is a real repeated call in the binary (three shakes, three drones,
 * one damage burst per member, one note per key) and is played.
 */
bool sfx_coalesces(SfxId);

/** What SfxPlayer::submit did with a request. */
enum class SfxAdmit : uint8_t {
    Started,     // the voice was idle
    Queued,      // behind the playing cue
    Coalesced,   // identical to the newest pending request: dropped
    Preempted,   // a scene/diagnostic cue replaced the playing one
    Overflowed,  // queued, and the OLDEST pending request was dropped for it
    Unsupported, // no A3-02 program (compile_sfx false)
    Stale,       // posted before the latest flush (transport epoch)
    Skipped      // A3-03: an ambient tick while the voice was busy (never queued)
};

class SfxPlayer {
  public:
    /**
     * Eight waiting cues: a burst of harpsichord keys plus the step after it
     * must not lose a note (the original's BIOS type-ahead held 15 keys, and
     * every one of them played). At most ~1.2 s of notes can lag.
     */
    static constexpr size_t kPendingDepth = 8;
    /** 2 ms: long enough not to click, short enough to be "immediate". */
    static constexpr uint32_t kReleaseFrames = kSfxOutputRateHz / 500;

    struct Stats {
        uint32_t started = 0, queued = 0, coalesced = 0, preempted = 0, overflowed = 0;
        uint32_t unsupported = 0, stale = 0, flushes = 0, completed = 0, ambient_skipped = 0;
    };

    /** The policy. Never blocks, never allocates. */
    SfxAdmit submit(const SfxRequest &);
    /**
     * Transport form (the device): a request carries the epoch it was posted
     * under; one older than the last sync_epoch() is dropped as Stale.
     */
    SfxAdmit submit(const SfxRequest &, uint32_t epoch);
    /** A flush happened on the game thread: flush here if the epoch moved. */
    void sync_epoch(uint32_t epoch);
    /** Cancel: the playing cue fades out over kReleaseFrames; pending cues are dropped. */
    void flush();

    /**
     * Fill `frames` samples: the voice at `gain_q15` (applied ONCE, saturating),
     * then silence. Returns the frames that carried a cue.
     */
    size_t render(int16_t *out, size_t frames, uint16_t gain_q15);

    bool idle() const { return !voice_.active() && pending_count_ == 0; }
    /** The playing cue, None when silent. */
    SfxId playing() const { return voice_.active() ? playing_.id : SfxId::None; }
    size_t pending() const { return pending_count_; }
    /** The pending request at FIFO position i (0 = next). */
    const SfxRequest &pending_at(size_t i) const { return pending_[(pending_head_ + i) % kPendingDepth]; }
    uint16_t noise_state() const { return noise_; }
    uint32_t epoch() const { return epoch_; }
    const Stats &stats() const { return stats_; }

  private:
    bool start_next();
    void start(const SfxRequest &);
    void push(const SfxRequest &);
    void drop_pending_below(SfxClass);

    SpeakerVoice voice_{};
    SfxRequest playing_{};
    SfxRequest pending_[kPendingDepth]{};
    size_t pending_head_ = 0, pending_count_ = 0;
    bool start_after_release_ = false;
    uint16_t noise_ = kNoiseSeed;
    uint16_t rumble_ = kRumbleSeed;
    uint32_t epoch_ = 0;
    Stats stats_{};
};

/**
 * The reference's combat derivation (game/src/core/sfx.ts sfxForCombatEvent):
 * the hit bursts are the kernel 0x3564 flash, chosen by the TARGET's side
 * (0x35ac `test [bx+2],0x80`). A miss is silent. A death is silent too
 * (A3-05): the killing blow's Attacked already carries 0x3564's burst, and
 * neither the strike COMBAT 0x194A nor the " killed!" branch (COMSUBS 0x036b)
 * reaches a speaker call. The reference's `died` -> combat-defeat cited 0x2fe3,
 * which is the chest trap 0x2fd0. PURE.
 */
SfxId sfx_for_combat_attack(bool died, int8_t hit, bool target_is_player);

} // namespace openu5
