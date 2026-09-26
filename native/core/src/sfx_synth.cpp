#include "openu5/sfx_synth.h"

#include "openu5/presentation.h"
#include "openu5/scene_timing.h"

// Alpha 3 A3-02. The speaker primitives, emulated from their loop bodies in
// ULTIMA.EXE (read for this batch with re/tools/dis16.py --exe):
//
//   tone_sweep 0x2192(a0 step, a1 start, a2 count, a3 delay, a4 inc)
//     PIT ch2 = 0x3c (a ~20 kHz carrier the cone averages), bx = start, dx = 0;
//     count times: dx += inc; gate = dx > bx (`cmp dx,bx; ja` 0x21f8, unsigned);
//     bx += step; spin a3 x C/24. So the pitch is inc/65536 per iteration and
//     start/step sweep the DUTY CYCLE -- the reference's constant-pitch reading
//     (speaker.ts toneSweep), which this port keeps, plus the duty it left out.
//   noise_burst 0x223c(a0 band, a1 dur, a2 step)
//     gate on; do { s = PRNG(s); PIT = 0x1234DE / (100 + s % (band - 99));
//     acc += step; spin step x C>>4 } while (acc < dur)   -- so ceil(dur/step)
//     iterations, at least one. The PRNG word [0x545c] never touches g_rng.
//   beep 0x22c0(a0 dur, a1 freq) = set_tone(freq); delay(dur, 1); stop.
//   glide 0x43ae(a0 total, a1 step, a2 end, a3 start)
//     inc = trunc16((end - start) * step) / total; si = start; di = 0;
//     while (di < total) { set_tone(si); delay(step, 1); si += inc; di += step }.
//   delay 0x20c8(a0 count, a1 shift): count x (C >> table[shift]); shift 1 = 0.
//
// C is the boot calibration [0x5356]; C x t_dec is machine-independent by
// design, and the one measured figure (speaker.ts) makes a tone_sweep
// iteration 1/25806 s. Everything below is in half sweep samples so that the
// noise unit (1.5 samples) stays an integer.
namespace openu5 {
namespace {

static_assert(kSpeakerSweepRate == kToneSweepSamplesPerSecond,
              "the synthesizer and the scene pacers share ONE calibration");
static_assert(kRumbleCycles == uint32_t(kQuakePulses) && kRumbleCycleMs == uint32_t(kQuakePeriodMs),
              "the quake's rumble lasts exactly the shake the device draws");
static_assert(kBlackthornSirenSamples == 2u * 460u * 0xc8u,
              "the Blackthorn siren is the shard ritual's two 460-call loops");

constexpr uint32_t kFineRate = kSfxOutputRateHz * kSfxOversample;
constexpr uint32_t kToneEdgeFrames = kSfxOutputRateHz / 250;  // 4 ms (speaker.ts EDGE_S)
constexpr uint32_t kNoiseEdgeFrames = kSfxOutputRateHz / 4000; // 0.25 ms (NOISE_EDGE_S 0.2 ms)
// The noise path at 16 kHz (speaker.ts: unipolar gate -> 20 Hz high-pass -> 2100 Hz
// low-pass; two-pole biquads there, one pole each here -- the timbre is class C):
// r = exp(-2*pi*20/16000), a = 1 - exp(-2*pi*2100/16000), in Q15.
constexpr int32_t kNoiseHighPassQ15 = 32512;
constexpr int32_t kNoiseLowPassQ15 = 18404;
constexpr uint32_t kHzMin = 20, kHzMax = 20000; // speaker.ts HZ_MIN/HZ_MAX

/** The PIT divisor the binary writes for value v (0x22f2 / 0x227b `div cx`). */
uint32_t pit_divisor(uint32_t value) {
    // speaker.ts clamps every pushed frequency to [20, 20000]. Below 19 Hz the
    // binary's 16-bit quotient would overflow (#DE); no A3-02 site gets there.
    const uint32_t v = value < kHzMin ? kHzMin : value > kHzMax ? kHzMax : value;
    const uint32_t d = kPitClockHz / v;
    return d ? d : 1;
}

/** 32-bit phase step at the fine rate for a PIT square of divisor d. */
uint32_t pit_phase_inc(uint32_t divisor) {
    return uint32_t((uint64_t(kPitClockHz) << 32) / (uint64_t(divisor) * kFineRate));
}

/** trunc16((end - start) * step) / total, as 0x43bc-0x43cc computes it. */
int16_t glide_increment(uint16_t start, uint16_t end, uint16_t step, int16_t total) {
    const int16_t diff = int16_t(uint16_t(end - start));
    const int16_t low = int16_t(uint16_t(int32_t(diff) * int32_t(int16_t(step)))); // imul, keep ax, cwd
    return total ? int16_t(int32_t(low) / int32_t(total)) : 0;                      // idiv truncates
}

// DATA.OVL tables (fileoff = DS + 0x10), verified for this batch.
constexpr uint16_t kInstrumentNotes[10] = {0x1eab, 0x0c2c, 0x0da9, 0x0f56, 0x103f,
                                           0x123c, 0x1478, 0x16fa, 0x1857, 0x1b53}; // DS 0x2746
constexpr uint16_t kArpeggioNotes[6] = {0x0a3c, 0x0a3c, 0x0a3c, 0x0e74, 0x0f3c, 0x1040}; // DS 0x3a26
constexpr uint16_t kCeremonyInc[9] = {8810, 7830, 7060, 6550, 5950, 5570, 5180, 4820, 4480};       // DS 0x4af6
constexpr uint16_t kCeremonyUp[9] = {2700, 3000, 1000, 100, 5000, 4000, 2500, 1000, 1};            // DS 0x4b08
constexpr uint16_t kCeremonyDown[9] = {32700, 31000, 37000, 45000, 31000, 34000, 36500, 39000, 42000}; // DS 0x4b1a
constexpr uint16_t kCeremonyStep[9] = {3, 2, 2, 2, 1, 1, 1, 1, 1};                                 // DS 0x4b2c
// A3-03 tables (re/tools/a3_03_cue_sites.py reads the loops; values from DATA.OVL).
// ORDAINED, CAST2 0x0ac6-0x0b02: 7 calls, inc/count/start/step from four parallel tables.
constexpr uint16_t kOrdainedInc[7] = {0x0ce4, 0x0f55, 0x0f55, 0x0f55, 0x0f55, 0x0e74, 0x0f55};   // DS 0x4be6
constexpr uint16_t kOrdainedCount[7] = {0x1b58, 0x1770, 0x0bb8, 0x0bb8, 0x0bb8, 0x0bb8, 0x1f40}; // DS 0x4bf4
constexpr uint16_t kOrdainedStart[7] = {0x03e8, 0x03e8, 0x03e8, 0x03e8, 0x03e8, 0x03e8, 0x01f4}; // DS 0x4c02
constexpr uint16_t kOrdainedStep[7] = {0x09, 0x0a, 0x15, 0x15, 0x15, 0x15, 0x08};                // DS 0x4c10
// Refuge "But thy slumber is disturbed!", BLCKTHRN 0x0a0d-0x0a49: 6 calls, the same layout.
constexpr uint16_t kSlumberInc[6] = {0x1130, 0x101d, 0x0e53, 0x0b75, 0x0ce4, 0x0ce4};   // DS 0x3720
constexpr uint16_t kSlumberCount[6] = {0xc350, 0xc350, 0xc350, 0x7530, 0x9c40, 0x9c40}; // DS 0x372c
constexpr uint16_t kSlumberStart[6] = {0x0bb8, 0x0bb8, 0x0bb8, 0x03e8, 0x0064, 0x9ca4}; // DS 0x3738
constexpr uint16_t kSlumberStep[6] = {1, 1, 1, 1, 1, 0xffff};                           // DS 0x3744
// The mirrored "two 460-call loops" (CAST 0x15dd-0x162a, CAST2 0x0bd0-0x0c0f and
// 0x0c44-0x0c83, BLCKTHRN 0x03d0-0x040f): si = 0x7d0 .. < 0x61a8 by +0x32, then
// 0x61a8 .. > 0x7d0 by -0x32; each call tone_sweep(inc, 1, count, si, 0).
constexpr uint16_t kLadderLow = 0x7d0, kLadderHigh = 0x61a8;
constexpr int16_t kLadderStride = 0x32;
constexpr uint16_t kLadderCalls = (kLadderHigh - kLadderLow + kLadderStride - 1) / kLadderStride; // 460
static_assert(kLadderCalls == 460, "each leg is 460 calls");

struct Builder {
    SpeakerProgram &p;
    void add(const SpeakerSegment &s) {
        if (p.count < kMaxSpeakerSegments) p.segments[p.count++] = s;
    }
};

int32_t clamp_index(int32_t v, int32_t hi) { return v < 0 ? 0 : v > hi ? hi : v; }

/** Both legs of a mirrored ladder (see kLadderLow). */
void add_ladder(Builder &b, uint16_t inc, uint16_t count) {
    b.add(speaker_sweep_loop(inc, 1, count, kLadderLow, 0, kLadderCalls, kLadderStride));
    b.add(speaker_sweep_loop(inc, 1, count, kLadderHigh, 0, kLadderCalls, int16_t(-kLadderStride)));
}

} // namespace

uint32_t pit_quantized_millihertz(uint32_t value) {
    return uint32_t(uint64_t(kPitClockHz) * 1000u / pit_divisor(value));
}

uint32_t sweep_millihertz(uint16_t inc) { return uint32_t(uint64_t(inc) * kSpeakerSweepRate * 1000u / 65536u); }

uint16_t speaker_noise_next(uint16_t s) {
    const uint16_t a = uint16_t(s + 0x9248);
    const uint16_t r = uint16_t((a >> 3) | (a << 13)); // ror ax, 3
    return uint16_t((r ^ 0x9248) + 0x11);
}

uint16_t speaker_noise_draw(uint16_t &state, uint16_t band) {
    state = speaker_noise_next(state);
    const uint16_t span = uint16_t(band - 0x64 + 1); // sub cx,bx; inc cx -- [100, band] CLOSED
    return uint16_t(0x64 + (span ? state % span : state));
}

const char *speaker_primitive_name(SpeakerPrimitive k) {
    switch (k) {
    case SpeakerPrimitive::None: return "none";
    case SpeakerPrimitive::Tone: return "tone";
    case SpeakerPrimitive::Glide: return "glide";
    case SpeakerPrimitive::Sweep: return "sweep";
    case SpeakerPrimitive::Noise: return "noise";
    case SpeakerPrimitive::Silence: return "silence";
    case SpeakerPrimitive::Rumble: return "rumble";
    }
    return "invalid";
}

uint16_t speaker_rumble_draw(uint16_t &state, uint16_t lo, uint16_t hi) {
    state = speaker_noise_next(state);                  // 0x2098-0x20aa: the same step as [0x545c]'s
    const uint16_t v = uint16_t(state & 0x7fff);        // 0x20ad and ax, 0x7fff
    const uint16_t span = uint16_t(hi - lo + 1);        // 0x20b6 sub cx,bx / inc cx
    return uint16_t(lo + (span ? v % span : v));        // 0x20bb div cx / add dx,bx
}

uint64_t SpeakerProgram::half_samples() const {
    uint64_t total = 0;
    for (uint8_t i = 0; i < count; ++i) total += uint64_t(segments[i].iterations) * segments[i].iteration_half_samples;
    return total;
}

SpeakerSegment speaker_beep(uint16_t freq, uint16_t dur) {
    SpeakerSegment s;
    s.kind = SpeakerPrimitive::Tone;
    s.value = freq;
    s.iterations = 1;
    s.iteration_half_samples = uint32_t(dur) * kDelayUnitHalfSamples;
    return s;
}

SpeakerSegment speaker_glide(uint16_t start, uint16_t end, uint16_t step, int16_t total) {
    SpeakerSegment s;
    s.kind = SpeakerPrimitive::Glide;
    s.value = start;
    s.delta = glide_increment(start, end, step, total);
    // `jl` is signed (0x43ef): total <= 0 runs no pass at all -- a mute glide.
    // step 0 would hang the binary; no site pushes it.
    s.iterations = total > 0 && step > 0 ? (uint32_t(total) + step - 1) / step : 0;
    s.iteration_half_samples = uint32_t(step) * kDelayUnitHalfSamples;
    return s;
}

SpeakerSegment speaker_sweep(uint16_t inc, uint16_t delay, uint16_t count, uint16_t start, int16_t step) {
    SpeakerSegment s;
    s.kind = SpeakerPrimitive::Sweep;
    s.value = inc;
    s.start = start;
    s.delta = step;
    // `loop` with cx = 0 would run 65536 times; no site pushes 0.
    s.iterations = count;
    s.iteration_half_samples = 2u * (delay ? delay : 1u);
    return s;
}

SpeakerSegment speaker_noise(uint16_t step, uint16_t dur, uint16_t band) {
    SpeakerSegment s;
    s.kind = SpeakerPrimitive::Noise;
    s.value = band;
    const uint32_t st = step ? step : 1;
    const uint32_t n = (uint32_t(dur) + st - 1) / st;
    s.iterations = n ? n : 1; // do-while: at least one draw
    s.iteration_half_samples = st * kNoiseUnitHalfSamples;
    return s;
}

SpeakerSegment speaker_silence(uint16_t count) {
    SpeakerSegment s;
    s.kind = SpeakerPrimitive::Silence;
    s.iterations = 1;
    s.iteration_half_samples = uint32_t(count) * kDelayUnitHalfSamples;
    return s;
}

SpeakerSegment speaker_fixed_tone(uint16_t hz, uint32_t milliseconds) {
    SpeakerSegment s;
    s.kind = SpeakerPrimitive::Tone;
    s.value = hz;
    s.iterations = 1;
    s.iteration_half_samples = uint32_t(uint64_t(milliseconds) * kSpeakerHalfSampleRate / 1000u);
    return s;
}

SpeakerSegment speaker_sweep_loop(uint16_t inc, uint16_t delay, uint16_t count, uint16_t start, int16_t step,
                                  uint16_t calls, int16_t stride) {
    SpeakerSegment s = speaker_sweep(inc, delay, count, start, step);
    s.calls = calls ? calls : 1;
    s.call_stride = stride;
    s.iterations = uint32_t(count) * s.calls;
    return s;
}

SpeakerSegment speaker_rumble(uint16_t lo, uint16_t hi, uint32_t milliseconds) {
    SpeakerSegment s;
    s.kind = SpeakerPrimitive::Rumble;
    s.value = lo;
    s.start = hi;
    s.iterations = 1;
    s.iteration_half_samples = uint32_t(uint64_t(milliseconds) * kSpeakerHalfSampleRate / 1000u);
    return s;
}

SpeakerSegment speaker_ramp(uint16_t start, int16_t delta, uint32_t steps, uint16_t delay) {
    SpeakerSegment s;
    s.kind = SpeakerPrimitive::Glide;
    s.value = start;
    s.delta = delta;
    s.iterations = steps;
    s.iteration_half_samples = uint32_t(delay) * kDelayUnitHalfSamples;
    return s;
}

uint32_t speaker_segment_frames(const SpeakerSegment &s) {
    return uint32_t(uint64_t(s.iterations) * s.iteration_half_samples * kSfxOutputRateHz / kSpeakerHalfSampleRate);
}

uint32_t speaker_program_frames(const SpeakerProgram &p) {
    uint32_t total = 0;
    for (uint8_t i = 0; i < p.count; ++i) total += speaker_segment_frames(p.segments[i]);
    return total;
}

bool sfx_supported(SfxId id) {
    switch (id) {
    case SfxId::MoveBlocked: case SfxId::MoveStep:
    case SfxId::CombatHit: case SfxId::CombatHitHeavy: case SfxId::CombatDamage: case SfxId::CombatDefeat:
    case SfxId::DungeonTrap: case SfxId::DungeonZap: case SfxId::FieldAfflict: case SfxId::MirrorBreak:
    case SfxId::DungeonFail: case SfxId::TorchBorrowed: case SfxId::RingVanishes: case SfxId::CannonFire:
    case SfxId::WaterfallFall:
    case SfxId::TimeSpell:
    case SfxId::InstrumentNote:
    case SfxId::ApparitionMaterialize: case SfxId::ApparitionArpeggio: case SfxId::ApparitionHealChime:
    case SfxId::ApparitionChord: case SfxId::BlackthornMaterialize:
    case SfxId::DiagnosticTone:
    // A3-03
    case SfxId::VictoryFanfare: case SfxId::ShardSweep: case SfxId::ShardNoEffect:
    case SfxId::ShrineDonation: case SfxId::ShrineOrdained: case SfxId::ShrineWellDone:
    case SfxId::Moongate: case SfxId::Sceptre: case SfxId::SceptreReclaimed: case SfxId::ShadowlordAnnounce:
    case SfxId::Quake: case SfxId::RefugeThunder: case SfxId::RefugeSlumber: case SfxId::RefugeRevival:
    case SfxId::ShopTransaction: case SfxId::SpellZap:
    case SfxId::CombatEscape: case SfxId::CombatAbsorbed: case SfxId::CombatCharm: case SfxId::CombatSummon:
    case SfxId::CombatGrazed: case SfxId::CombatDraggedUnder: case SfxId::CombatEngulfed:
    case SfxId::CombatRegurgitated: case SfxId::CombatFoodStolen:
    case SfxId::ShipCollision: case SfxId::ShipSinking: case SfxId::TheftDetected: case SfxId::WishGranted:
    case SfxId::TrapdoorFall: case SfxId::SearchFail:
    case SfxId::AmbientFountain: case SfxId::AmbientWaterfall: case SfxId::AmbientClockTick:
    case SfxId::AmbientClockTock: case SfxId::AmbientClockChime:
    case SfxId::IntroThunder: case SfxId::IntroChime: case SfxId::IntroSummon:
        return true;
    default:
        return false;
    }
}

// Each row: the site and the arguments IN PUSH ORDER, as the reference and
// re/tools/a3_02_cue_sites.py print them.
bool compile_sfx(SfxId id, int32_t param, SpeakerProgram &program) {
    program = {};
    Builder b{program};
    switch (id) {
    // -- world ---------------------------------------------------------------
    case SfxId::MoveBlocked: // beep(0xa5, 0xc8): MAINOUT 0x0344, TOWN 0x0849
        b.add(speaker_beep(0xa5, 0xc8));
        break;
    case SfxId::MoveStep: // sfx_footstep kernel 0x433e: NB(1,0x19,0x3e8); delay(0x14,1); NB(1,0x19,0x5dc)
        b.add(speaker_noise(1, 0x19, 0x3e8));
        b.add(speaker_silence(0x14));
        b.add(speaker_noise(1, 0x19, 0x5dc));
        break;
    case SfxId::TorchBorrowed: // glide(0x320 -> 0x7d0, 1, 0x32): SJOG 0x1a21
    case SfxId::DungeonFail:   // the same four pushes: DUNGEON 0x1cfb
        b.add(speaker_glide(0x320, 0x7d0, 1, 0x32));
        break;
    case SfxId::RingVanishes: // glide(0x4b0 -> 0x7d0, 1, 0x28): ZSTATS 0x0e42
        b.add(speaker_glide(0x4b0, 0x7d0, 1, 0x28));
        break;
    case SfxId::CannonFire: // glide(0x3e8 -> 0xc8, 5, 0x12c): CMDS 0x09d5 / 0x0c05
        b.add(speaker_glide(0x3e8, 0xc8, 5, 0x12c));
        break;
    case SfxId::WaterfallFall: // glide(0x9c4 -> 0x320, 1, 0x12c): OUTSUBS 0x0492
        b.add(speaker_glide(0x9c4, 0x320, 1, 0x12c));
        break;
    case SfxId::DungeonTrap: // the 0x2fd0 dispatcher: NB(0x28,0xbb8,0x1f4) @0x2fe3
        b.add(speaker_noise(0x28, 0xbb8, 0x1f4));
        break;
    case SfxId::DungeonZap: // NB(1,0x1f4,0x4e20): DUNGEON 0x04b9
        b.add(speaker_noise(1, 0x1f4, 0x4e20));
        break;
    case SfxId::FieldAfflict: // NB(1,0x32,0xdac): DUNGEON 0x099e / 0x0a30
        b.add(speaker_noise(1, 0x32, 0xdac));
        break;
    case SfxId::MirrorBreak: // TOWN 0x0a69-0x0a80: si = 0x7d0 .. <0x4e20 step 0x3e8, NB(0x28,0x78,si)
        for (uint16_t si = 0x7d0; si < 0x4e20; si = uint16_t(si + 0x3e8)) b.add(speaker_noise(0x28, 0x78, si));
        break;
    // -- combat (kernel 0x3564 flash, by the target's side) ---------------------
    case SfxId::CombatHit: // target is an enemy: NB(0xa,0xbb8,0x7d0) @0x35de
        b.add(speaker_noise(0xa, 0xbb8, 0x7d0));
        break;
    case SfxId::CombatHitHeavy: // target is a party member: NB(0x28,0xbb8,0x1f4) @0x35c9
    case SfxId::CombatDefeat:   // the 0x2fd0 burst: NB(0x28,0xbb8,0x1f4) @0x2fe3
        b.add(speaker_noise(0x28, 0xbb8, 0x1f4));
        break;
    case SfxId::CombatDamage: // party_member_take_damage 0x2a52: NB(0xa,0x640,0x7d0) @0x2a68
        b.add(speaker_noise(0xa, 0x640, 0x7d0));
        break;
    // -- magic: the ceremony CAST2.OVL 0x0000(idx), idx < 9 ----------------------
    case SfxId::TimeSpell: {
        const int32_t i = clamp_index(param, 8);
        b.add(speaker_noise(0x320, uint16_t(0x1f40 + 0x640 * i), 0x2bc)); // 0x000b-0x001d
        const uint16_t count = uint16_t(0x2710 + 0xfa0 * i);              // 0x0039-0x0041
        b.add(speaker_sweep(kCeremonyInc[i], 1, count, kCeremonyUp[i], int16_t(kCeremonyStep[i])));    // 0x0056
        b.add(speaker_sweep(kCeremonyInc[i], 1, count, kCeremonyDown[i], int16_t(-kCeremonyStep[i]))); // 0x006d
        break;
    }
    // -- the harpsichord: TOWN 0x0e34, tone_sweep(note[digit], 1, 0xfa0, 0x4e20, 0xfffc) @0x0e6d
    case SfxId::InstrumentNote:
        b.add(speaker_sweep(kInstrumentNotes[clamp_index(param, 9)], 1, 0xfa0, 0x4e20, int16_t(0xfffc)));
        break;
    // -- scenes ----------------------------------------------------------------
    case SfxId::ApparitionMaterialize: // OUTSUBS 0x067b: TS(0xa3c,1,0x2710,0x9c4,6)
        b.add(speaker_sweep(0xa3c, 1, 0x2710, 0x9c4, 6));
        break;
    case SfxId::ApparitionArpeggio: // OUTSUBS 0x0683-0x06a2: TS([si],1,0x1388,0xc8,0xd) x 6
        for (uint16_t note : kArpeggioNotes) b.add(speaker_sweep(note, 1, 0x1388, 0xc8, 0xd));
        break;
    case SfxId::ApparitionHealChime: // OUTSUBS 0x0896: TS(0x157c,1,0x1388,0xc8,0xd)
        b.add(speaker_sweep(0x157c, 1, 0x1388, 0xc8, 0xd));
        break;
    case SfxId::ApparitionChord: // OUTSUBS 0x08c1: TS(0x157c,1,0xea60,0x9c4,1)
        b.add(speaker_sweep(0x157c, 1, 0xea60, 0x9c4, 1));
        break;
    case SfxId::BlackthornMaterialize: // BLCKTHRN 0x083f: TS(0xaf0,1,0x32c8,0x64,5)
        b.add(speaker_sweep(0xaf0, 1, 0x32c8, 0x64, 5));
        break;
    // -- device diagnostics (class D): the A3-01 chime, through the same PIT law
    case SfxId::DiagnosticTone:
        b.add(speaker_fixed_tone(880, 120));
        b.add(speaker_fixed_tone(1320, 180));
        break;

    // == A3-03 ==================================================================
    // -- victory: sfx_victory_fanfare 0x4368 (callers COMBAT 0x0d02, CAST 0x1759)
    case SfxId::VictoryFanfare:
        for (int i = 0; i < 3; ++i) b.add(speaker_sweep(0x11f8, 1, 0x2a30, 0x12c, 6)); // 0x4377-0x438f, si = 3
        b.add(speaker_sweep(0x17d4, 1, 0x5460, 0x12c, 3));                            // 0x4391-0x43a5
        break;
    // -- the mirrored ladders (see kLadderLow)
    case SfxId::ShardSweep: // CAST 0x15dd-0x162a; the Blackthorn siren BLCKTHRN 0x03d0-0x040f is the same call
        add_ladder(b, 0xa50, 0xc8);
        break;
    case SfxId::ShrineDonation: // CAST2 0x0bd0-0x0c0f (ALAKAZAM)
        add_ladder(b, 0xa8c, 0xc8);
        break;
    case SfxId::ShrineWellDone: // CAST2 0x0c44-0x0c83 (the shake 0x0c88 follows as its own cue)
        add_ladder(b, 0xc1c, 0x96);
        break;
    case SfxId::ShrineOrdained: // CAST2 0x0ac6-0x0b02
        for (int i = 0; i < 7; ++i)
            b.add(speaker_sweep(kOrdainedInc[i], 1, kOrdainedCount[i], kOrdainedStart[i], int16_t(kOrdainedStep[i])));
        break;
    case SfxId::ShardNoEffect:  // CAST 0x1656-0x166d "No effect!" (the wrong flame)
    case SfxId::CombatFoodStolen: // COMBAT 0x0394-0x03b6 " stole some food!"
    case SfxId::TheftDetected:  // TALK 0x1191-0x11a8 "Something was stolen!"
        b.add(speaker_glide(0x320, 0x7d0, 1, 0x32));
        break;
    // -- world
    case SfxId::Moongate: // kernel 0x48d1-0x48e5 (tile under the party == 0xdc)
        b.add(speaker_sweep(0x170c, 1, 0x7530, 0x7d0, 2));
        break;
    case SfxId::Sceptre: { // CAST 0x197b-0x198f after "Wielding the Sceptre...", then NB per dissolved field @0x19e0
        b.add(speaker_sweep(0x1450, 1, 0xc350, 0x1388, 1));
        const int32_t fields = clamp_index(param, 9);
        for (int32_t i = 0; i < fields; ++i) b.add(speaker_noise(0xa, 0xbb8, 0x7d0));
        break;
    }
    case SfxId::SceptreReclaimed: // kernel 0x6209-0x6221 "The Sceptre is reclaimed!"
        b.add(speaker_sweep(0xfd2, 1, 0xfde8, 1, 1));
        break;
    case SfxId::ShadowlordAnnounce: // TOWN 0x11d5-0x11e9
        b.add(speaker_sweep(0x19c8, 1, 0xea60, 0x7d0, 1));
        break;
    case SfxId::Quake:        // screen_shake_rumble 0x3072 (one call per cue)
    case SfxId::RefugeThunder: // BLCKTHRN 0x0acc / 0x0acf: the same 0x3072, one per peal
        b.add(speaker_rumble(kRumbleLowest, kRumbleHighest, kRumbleCycles * kRumbleCycleMs));
        break;
    case SfxId::ShopTransaction: // the healer's service, SHOPPES 0x13b0-0x1469
        b.add(speaker_sweep(0x100e, 1, 0x57e4, 0x1388, 1));
        b.add(speaker_sweep(0x100e, 1, 0x57e4, 0x6b6c, -1));
        b.add(speaker_sweep(0x11b2, 1, 0x9c40, 1, 1));
        b.add(speaker_sweep(0x11b2, 1, 0x9c40, 0x9c40, -1));
        b.add(speaker_sweep(0x8fc, 1, 0x4650, 1, 2));
        b.add(speaker_sweep(0x8fc, 1, 0x4650, 0x8ca0, -2));
        break;
    case SfxId::SpellZap: // "Absorbed!": CAST 0x0e53-0x0e6e, COMBAT 0x093d-0x0958 (and 0x0d85)
        b.add(speaker_sweep(0x2648, 1, 0x6d60, 0x3e8, 2));
        break;
    case SfxId::ShipCollision: // MAINOUT 0x02f4-0x0300 "COLLISION!" (not "Docked!")
        b.add(speaker_noise(0x64, 0x7d0, 0x12c));
        break;
    case SfxId::ShipSinking: // MAINOUT 0x112b-0x113b (no skiff) / 0x1296-0x12a6 (WHIRLPOOL)
        b.add(speaker_glide(0x294, 0x96, 0x28, 0x1e78));
        break;
    case SfxId::WishGranted: // LOOKOBJ 0x0116-0x0129 "Poof!"
        b.add(speaker_noise(0xa, 0xbb8, 0x7d0));
        break;
    case SfxId::SearchFail: // SJOG 0x01f2 search_remains_outcome: "Plague!" then NB(0x28,0xbb8,0x1f4) @0x0237
        b.add(speaker_noise(0x28, 0xbb8, 0x1f4));
        break;
    case SfxId::TrapdoorFall: { // TOWN 0x0fb3-0x0fd3: set_tone(v), delay(0x28,1), v-- while v > 0xfa
        b.add(speaker_ramp(0x3e8, -1, 0x3e8 - 0xfa, 0x28));
        const int32_t members = clamp_index(param, 6); // 0x1005-0x103a: one NB per member as it dies
        for (int32_t i = 0; i < members; ++i) b.add(speaker_noise(0x28, 0xbb8, 0x1f4));
        break;
    }
    // -- combat messages
    case SfxId::CombatEscape:     // SJOG 0x1c37 / CMDS 0x18ac
    case SfxId::CombatAbsorbed:   // SJOG 0x1f08
    case SfxId::CombatGrazed:     // COMSUBS 0x0352
    case SfxId::CombatDraggedUnder: // COMSUBS 0x03d6
        b.add(speaker_glide(0x4b0, 0x7d0, 1, 0x28));
        break;
    case SfxId::CombatCharm: // SJOG 0x2218 " passes out!", COMSUBS 0x01b1 " possessed!"
        b.add(speaker_sweep(0xc1c, 1, 0x7530, 0x3e8, 2));
        break;
    case SfxId::CombatSummon: // COMSUBS 0x02cb " gates in a daemon!"
        b.add(speaker_sweep(0xac8, 1, 0x1388, 0x3e8, 0xf));
        break;
    case SfxId::CombatEngulfed: // COMBAT 0x07de-0x07f1 "ARGH!"
        b.add(speaker_noise(0x28, 0xbb8, 0x1f4));
        break;
    case SfxId::CombatRegurgitated: // COMBAT 0x1cac-0x1cbf " regurgitated!"
        b.add(speaker_noise(1, 0x1b58, 0x258));
        break;
    // -- the Refuge (BLCKTHRN party_refuge)
    case SfxId::RefugeSlumber: // 0x0a0d-0x0a49 after "But thy slumber is disturbed!"
        for (int i = 0; i < 6; ++i)
            b.add(speaker_sweep(kSlumberInc[i], 1, kSlumberCount[i], kSlumberStart[i], int16_t(kSlumberStep[i])));
        break;
    case SfxId::RefugeRevival: { // 0x0b5d-0x0bb0: per member, inc = 0x8e30 / (i + 7) (ldiv kernel 0x03a0)
        const int32_t members = param < 1 ? 1 : clamp_index(param, 6); // [0x585b] >= 1 at the Refuge
        for (int32_t i = 0; i < members; ++i) b.add(speaker_sweep(uint16_t(0x8e30 / (i + 7)), 1, 0x7530, 0x7d0, 2));
        break;
    }
    // -- ambient proximity: ambient_sfx_tick 0x4102, one call per redraw
    case SfxId::AmbientFountain: // class 3, 0x42c4
        b.add(speaker_noise(0xa, 0x1e, 0x61a8));
        break;
    case SfxId::AmbientWaterfall: // class 2, 0x42b2-0x42be
        b.add(speaker_noise(0x14, 0x3c, 0x2710));
        break;
    case SfxId::AmbientClockTick: // class 1, phase 0, 0x4299-0x42a1
        b.add(speaker_beep(0xbb8, 3));
        break;
    case SfxId::AmbientClockTock: // class 1, phase 4, 0x42ad -> 0x429c
        b.add(speaker_beep(0x7d0, 3));
        break;
    case SfxId::AmbientClockChime: // class 1 while [0x5884] != 0, 0x4277-0x428b
        b.add(speaker_sweep(0xc2c, 1, 0x7d0, 0x4e20, int16_t(0xfff6)));
        break;
    // -- the intro's scene engine (FONT.OVL, shared by INTRO)
    case SfxId::IntroThunder: // FONT 0x03c6-0x03ca: NB(0x14,0x3c,0x2710)
        b.add(speaker_noise(0x14, 0x3c, 0x2710));
        break;
    case SfxId::IntroChime: // FONT 0x03e8-0x0403: beep(0xbb8 at frame 0, 0x7d0 at frame 4, 3)
        b.add(speaker_beep(param == 4 ? 0x7d0 : 0xbb8, 3));
        break;
    case SfxId::IntroSummon: // FONT 0x0881-0x088d: NB(1,0x4b0,0xfa0)
        b.add(speaker_noise(1, 0x4b0, 0xfa0));
        break;
    default:
        return false;
    }
    return program.count > 0;
}

// ---------------------------------------------------------------------------
// SpeakerVoice
// ---------------------------------------------------------------------------
void SpeakerVoice::start(const SpeakerProgram &program, uint16_t *noise_state, uint16_t *rumble_state) {
    program_ = program;
    noise_ = noise_state;
    rumble_ = rumble_state ? rumble_state : &own_rumble_;
    index_ = 0;
    position_ = 0;
    release_total_ = release_left_ = 0;
    active_ = program_.count > 0 && begin_segment();
}

bool SpeakerVoice::begin_segment() {
    while (index_ < program_.count) {
        const auto &s = program_.segments[index_];
        frames_ = speaker_segment_frames(s);
        if (frames_ == 0 || s.kind == SpeakerPrimitive::None) {
            ++index_;
            continue;
        }
        frame_ = 0;
        fine_ = 0;
        iteration_ = 0;
        const uint32_t e = s.kind == SpeakerPrimitive::Noise ? kNoiseEdgeFrames : kToneEdgeFrames;
        edge_ = e < frames_ / 2 ? e : frames_ / 2;
        if (!edge_) edge_ = 1;
        phase_ = 0;
        dx_ = 0;
        bx_ = s.start;
        value_ = s.value;
        hp_in_ = hp_out_ = lp_out_ = 0;
        rumble_phase_ = rumble_half_ = 0;
        gate_ = false;
        begin_iteration();
        next_boundary_ = uint64_t(1) * s.iteration_half_samples * kFineRate / kSpeakerHalfSampleRate;
        return true;
    }
    return false;
}

// One iteration of the primitive's own loop: what the binary does at the top
// of each pass, before it spins the pass's delay.
void SpeakerVoice::begin_iteration() {
    const auto &s = program_.segments[index_];
    switch (s.kind) {
    case SpeakerPrimitive::Tone:
        phase_inc_ = pit_phase_inc(pit_divisor(s.value));
        break;
    case SpeakerPrimitive::Glide:
        if (iteration_) value_ = uint16_t(value_ + s.delta); // si += inc (after the set_tone)
        phase_inc_ = pit_phase_inc(pit_divisor(value_));
        break;
    case SpeakerPrimitive::Sweep:
        if (s.calls > 1 && iteration_ && iteration_ % (s.iterations / s.calls) == 0) {
            // A3-03: the next call of a ladder loop re-enters tone_sweep:
            // dx = 0, bx = the loop's next start.
            dx_ = 0;
            bx_ = uint16_t(s.start + int32_t(s.call_stride) * int32_t(iteration_ / (s.iterations / s.calls)));
        }
        dx_ = uint16_t(dx_ + s.value); // add dx, inc
        gate_ = dx_ > bx_;             // cmp dx, bx; ja -> gate on
        threshold_ = bx_;
        bx_ = uint16_t(bx_ + s.delta); // add bx, step
        break;
    case SpeakerPrimitive::Rumble: // the half-cycles are drawn in fine_level()
        break;
    case SpeakerPrimitive::Noise:
        value_ = speaker_noise_draw(*noise_, s.value);
        phase_inc_ = pit_phase_inc(pit_divisor(value_));
        break;
    case SpeakerPrimitive::Silence:
    case SpeakerPrimitive::None:
        break;
    }
}

// One sample at the fine rate, in Q15 of the square's peak.
int32_t SpeakerVoice::fine_level() {
    const auto kind = program_.segments[index_].kind;
    int32_t level = 0;
    switch (kind) {
    case SpeakerPrimitive::Tone:
    case SpeakerPrimitive::Glide:
        level = phase_ < 0x80000000u ? 32767 : -32768; // PIT mode-3 square, bipolar (DC-free)
        break;
    case SpeakerPrimitive::Noise:
        level = phase_ < 0x80000000u ? 32767 : 0; // the gate only pushes the cone
        break;
    case SpeakerPrimitive::Sweep: {
        // The 1-bit PWM, with its expected level (the duty) removed so the
        // output stays centred however far the duty sweeps.
        const int32_t duty = int32_t((65535u - threshold_) >> 1);
        level = (gate_ ? 32767 : 0) - duty;
        level *= 2;
        break;
    }
    case SpeakerPrimitive::Rumble: {
        // 8253 mode 3 with a count written every few hundred microseconds and
        // no control word: the output finishes its half-cycle, then reloads
        // the newest count. So every half-cycle is 1 / (2 v) s for a fresh
        // draw v in [lo, hi] (PIT-quantised), the gate held between flips.
        const auto &s = program_.segments[index_];
        if (!rumble_half_) rumble_half_ = uint64_t(pit_divisor(speaker_rumble_draw(*rumble_, s.value, s.start))) * kFineRate;
        while (rumble_phase_ >= rumble_half_) {
            rumble_phase_ -= rumble_half_;
            gate_ = !gate_;
            rumble_half_ = uint64_t(pit_divisor(speaker_rumble_draw(*rumble_, s.value, s.start))) * kFineRate;
        }
        rumble_phase_ += 2ull * kPitClockHz; // half a divisor per flip
        level = gate_ ? 32767 : -32768;
        break;
    }
    case SpeakerPrimitive::Silence:
    case SpeakerPrimitive::None:
        break;
    }
    phase_ += phase_inc_;
    return level;
}

size_t SpeakerVoice::render(int32_t *out, size_t frames) {
    size_t produced = 0;
    while (produced < frames && active_) {
        if (frame_ >= frames_) {
            ++index_;
            if (!begin_segment()) {
                active_ = false;
                break;
            }
        }
        const auto &s = program_.segments[index_];
        int32_t sum = 0;
        for (uint32_t k = 0; k < kSfxOversample; ++k) {
            while (fine_ >= next_boundary_ && iteration_ + 1 < s.iterations) {
                ++iteration_;
                begin_iteration();
                next_boundary_ = uint64_t(iteration_ + 1) * s.iteration_half_samples * kFineRate / kSpeakerHalfSampleRate;
            }
            sum += fine_level();
            ++fine_;
        }
        int32_t v = int32_t((int64_t(sum) / int32_t(kSfxOversample)) * kSpeakerAmplitude / 32768);
        if (s.kind == SpeakerPrimitive::Noise) {
            // DC block (the AC-coupled cone) then the cone's low-pass.
            hp_out_ = v - hp_in_ + int32_t((int64_t(hp_out_) * kNoiseHighPassQ15) / 32768);
            hp_in_ = v;
            lp_out_ += int32_t((int64_t(hp_out_ - lp_out_) * kNoiseLowPassQ15) / 32768);
            v = lp_out_;
        }
        if (s.kind == SpeakerPrimitive::Silence) v = 0;
        // Edges: a linear ramp in and out of every segment, so the amplifier
        // never sees a step.
        const uint32_t in = frame_ + 1, left = frames_ - frame_;
        const uint32_t env = in < edge_ ? in : left < edge_ ? left : edge_;
        v = int32_t(int64_t(v) * env / edge_);
        if (release_total_) {
            v = int32_t(int64_t(v) * release_left_ / release_total_);
            if (release_left_) --release_left_;
        }
        out[produced++] = v;
        ++frame_;
        ++position_;
        if (release_total_ && release_left_ == 0) {
            active_ = false;
            release_total_ = 0;
        }
    }
    return produced;
}

void SpeakerVoice::release(uint32_t frames) {
    if (!active_ || release_total_) return;
    release_total_ = release_left_ = frames ? frames : 1;
}

// ---------------------------------------------------------------------------
// Policy
// ---------------------------------------------------------------------------
SfxClass sfx_class(SfxId id) {
    switch (id) {
    case SfxId::DiagnosticTone:
        return SfxClass::Diagnostic;
    // A3-03: the Shadowlord drone left this list -- it is no scene's beat, and
    // Stonegate's three announces must each be heard (FIFO), not cut.
    case SfxId::BlackthornMaterialize: case SfxId::ApparitionMaterialize:
    case SfxId::ApparitionArpeggio: case SfxId::ApparitionHealChime: case SfxId::ApparitionChord:
    case SfxId::RefugeThunder: case SfxId::RefugeSlumber: case SfxId::RefugeRevival:
    case SfxId::IntroThunder: case SfxId::IntroChime: case SfxId::IntroSummon:
    case SfxId::TitleFizzle: case SfxId::TitleCrackle: case SfxId::BardSong: case SfxId::EndgameOrb:
        return SfxClass::Scene;
    case SfxId::CastSpell: case SfxId::SpellZap: case SfxId::LineSpray: case SfxId::TimeSpell:
    case SfxId::ShrineWellDone: case SfxId::ShrineDonation: case SfxId::ShrineOrdained: case SfxId::ShardSweep:
    case SfxId::ShardNoEffect: case SfxId::Sceptre:
    case SfxId::SpellCast: case SfxId::PotionUsed: case SfxId::ScrollUsed: case SfxId::InvalidMagic:
        return SfxClass::Spell;
    case SfxId::CombatHit: case SfxId::CombatHitHeavy: case SfxId::CombatDamage: case SfxId::CombatDefeat:
    case SfxId::CombatEscape: case SfxId::CombatReject: case SfxId::CombatAbsorbed: case SfxId::VictoryFanfare:
    case SfxId::CombatCharm: case SfxId::CombatSummon: case SfxId::CombatGrazed: case SfxId::CombatDraggedUnder:
    case SfxId::CombatEngulfed: case SfxId::CombatRegurgitated: case SfxId::CombatFoodStolen:
        return SfxClass::Combat;
    case SfxId::InstrumentNote:
        return SfxClass::Instrument;
    case SfxId::AmbientFountain: case SfxId::AmbientWaterfall: case SfxId::AmbientClockTick:
    case SfxId::AmbientClockTock: case SfxId::AmbientClockChime:
        return SfxClass::Ambient;
    default:
        return SfxClass::Ordinary;
    }
}

bool sfx_coalesces(SfxId id) { return id == SfxId::MoveStep || id == SfxId::MoveBlocked; }

const char *sfx_class_name(SfxClass c) {
    switch (c) {
    case SfxClass::Ambient: return "ambient";
    case SfxClass::Ordinary: return "ordinary";
    case SfxClass::Instrument: return "instrument";
    case SfxClass::Combat: return "combat";
    case SfxClass::Spell: return "spell";
    case SfxClass::Scene: return "scene";
    case SfxClass::Diagnostic: return "diagnostic";
    }
    return "invalid";
}

void SfxPlayer::start(const SfxRequest &r) {
    SpeakerProgram program;
    compile_sfx(r.id, r.param, program);
    playing_ = r;
    voice_.start(program, &noise_, &rumble_);
    ++stats_.started;
}

bool SfxPlayer::start_next() {
    if (!pending_count_) return false;
    const SfxRequest next = pending_[pending_head_];
    pending_head_ = (pending_head_ + 1) % kPendingDepth;
    --pending_count_;
    start(next);
    return true;
}

void SfxPlayer::push(const SfxRequest &r) {
    if (pending_count_ == kPendingDepth) {
        // Overflow drops the OLDEST pending request (A3-01 contract), never the playing one.
        pending_head_ = (pending_head_ + 1) % kPendingDepth;
        --pending_count_;
        ++stats_.overflowed;
    }
    pending_[(pending_head_ + pending_count_) % kPendingDepth] = r;
    ++pending_count_;
}

void SfxPlayer::drop_pending_below(SfxClass floor) {
    SfxRequest keep[kPendingDepth]{};
    size_t kept = 0;
    for (size_t i = 0; i < pending_count_; ++i) {
        const auto &r = pending_at(i);
        if (sfx_class(r.id) >= floor) keep[kept++] = r;
    }
    pending_head_ = 0;
    pending_count_ = kept;
    for (size_t i = 0; i < kept; ++i) pending_[i] = keep[i];
}

SfxAdmit SfxPlayer::submit(const SfxRequest &r) {
    if (!sfx_supported(r.id)) {
        ++stats_.unsupported;
        return SfxAdmit::Unsupported;
    }
    const SfxClass cls = sfx_class(r.id);
    if (cls == SfxClass::Ambient) {
        // A3-03: ambience is the lowest class and never queues. The original
        // called it from the redraw only while nothing else was sounding
        // (every other primitive blocked), so a busy voice skips this tick.
        if (voice_.active() || pending_count_) {
            ++stats_.ambient_skipped;
            return SfxAdmit::Skipped;
        }
        start(r);
        return SfxAdmit::Started;
    }
    if (voice_.active() && sfx_class(playing_.id) == SfxClass::Ambient && pending_count_ == 0) {
        // Any real cue cuts a playing ambient tick at once (2 ms fade).
        voice_.release(kReleaseFrames);
        push(r);
        ++stats_.preempted;
        return SfxAdmit::Preempted;
    }
    if (cls >= SfxClass::Scene) {
        // A scene cue follows its pacer, so the latest one wins: it cuts
        // whatever is playing at its own class or below, and the lower-class
        // cues still queued behind it are stale by the time it ends.
        drop_pending_below(cls);
        if (!voice_.active()) {
            start(r);
            return SfxAdmit::Started;
        }
        if (sfx_class(playing_.id) <= cls) {
            voice_.release(kReleaseFrames);
            // Put it at the FRONT: it starts the moment the fade ends.
            if (pending_count_ == kPendingDepth) --pending_count_; // drop the newest
            pending_head_ = (pending_head_ + kPendingDepth - 1) % kPendingDepth;
            pending_[pending_head_] = r;
            ++pending_count_;
            ++stats_.preempted;
            return SfxAdmit::Preempted;
        }
    }
    if (!voice_.active() && pending_count_ == 0) {
        start(r);
        return SfxAdmit::Started;
    }
    // The speaker played one effect at a time and every primitive blocked, so
    // effects follow each other (FIFO). A held key's repeat of the newest
    // pending step / bump adds nothing (A3-03: only those; every other repeat
    // is a repeated call in the binary -- a note, a shake, a drone, a burst).
    if (sfx_coalesces(r.id) && pending_count_) {
        const auto &newest = pending_at(pending_count_ - 1);
        if (newest.id == r.id && newest.param == r.param) {
            ++stats_.coalesced;
            return SfxAdmit::Coalesced;
        }
    }
    const bool full = pending_count_ == kPendingDepth;
    push(r);
    ++stats_.queued;
    return full ? SfxAdmit::Overflowed : SfxAdmit::Queued;
}

SfxAdmit SfxPlayer::submit(const SfxRequest &r, uint32_t epoch) {
    if (epoch != epoch_) {
        ++stats_.stale;
        return SfxAdmit::Stale;
    }
    return submit(r);
}

void SfxPlayer::sync_epoch(uint32_t epoch) {
    if (epoch == epoch_) return;
    epoch_ = epoch;
    flush();
}

void SfxPlayer::flush() {
    pending_head_ = pending_count_ = 0;
    voice_.release(kReleaseFrames);
    ++stats_.flushes;
}

size_t SfxPlayer::render(int16_t *out, size_t frames, uint16_t gain_q15) {
    size_t done = 0, sounding = 0;
    int32_t scratch[64];
    while (done < frames) {
        if (!voice_.active() && !start_next()) {
            for (; done < frames; ++done) out[done] = 0;
            break;
        }
        const size_t want = frames - done < 64 ? frames - done : 64;
        const bool was_releasing = voice_.releasing();
        const size_t got = voice_.render(scratch, want);
        for (size_t i = 0; i < got; ++i) {
            const int32_t v = scratch[i] < -32768 ? -32768 : scratch[i] > 32767 ? 32767 : scratch[i];
            out[done + i] = apply_gain_q15(int16_t(v), gain_q15);
        }
        done += got;
        sounding += got;
        if (!voice_.active() && !was_releasing && !voice_.releasing()) ++stats_.completed;
    }
    return sounding;
}

SfxId sfx_for_combat_attack(bool died, int8_t hit, bool target_is_player) {
    if (died) return SfxId::CombatDefeat;
    if (hit > 0) return target_is_player ? SfxId::CombatHitHeavy : SfxId::CombatHit;
    return SfxId::None;
}

} // namespace openu5
