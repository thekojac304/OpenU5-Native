#include "openu5/music_synth.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>

#include "openu5/audio.h" // apply_gain_q15

// Alpha 3 A3-04. Port of game/src/ui/opl/{bank,voices,chip,sequencer}.ts and
// the EVNT half of extractor/src/audio/xmi2midi.ts -- see music_synth.h for
// what changed and why (nothing observable). Comments below say "(TS: ...)"
// where it helps to point at the line being ported; they are not a
// line-count reference, just an anchor for anyone diffing against the
// reference the next time the patch corpus is re-measured.
namespace openu5 {
namespace {

constexpr double kPi = 3.14159265358979323846;

// A3-04B (ALPHA3_AUDIO.md section 19.7): the per-operator helpers are part of
// the per-sample loop's body. GCC's size heuristic kept wave_atten() and
// advance_clocks() out of line at -O2 (two call sites; one call per sample):
// a call8/entry/retw round trip per operator per chip sample on the device.
// Forcing them inline changes code generation only.
#if defined(__GNUC__)
#define OPENU5_SYNTH_INLINE inline __attribute__((always_inline))
#else
#define OPENU5_SYNTH_INLINE inline
#endif

constexpr int32_t kEgInc[4][8] = {
    {0, 1, 0, 1, 0, 1, 0, 1},
    {0, 1, 0, 1, 1, 1, 0, 1},
    {0, 1, 1, 1, 0, 1, 1, 1},
    {0, 1, 1, 1, 1, 1, 1, 1},
};
constexpr int32_t kMult2[16] = {1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 20, 24, 24, 30, 30};
constexpr int32_t kVibratoSteps[8] = {0, 1, 2, 1, 0, -1, -2, -1};

// ---- OPL log-domain ROMs (chip.ts LOG_SIN / EXP): generated, not copied. ----
struct OplTables {
    uint16_t log_sin[256]{};
    uint16_t exp_tab[256]{};
    // A3-04B: kEgInc, kMult2 and kVibratoSteps again, copied here so the
    // per-sample path (the envelope step, the vibrato's phase steps) reads
    // them from internal RAM with the other two ROMs: on the device a
    // constexpr table lives in flash .rodata, behind the data cache the
    // renderer on the other core also uses (ALPHA3_AUDIO.md section 19.8).
    int8_t eg_inc[4][8]{};
    uint8_t mult2[16]{};
    int8_t vibrato_steps[8]{};
    OplTables() {
        for (int i = 0; i < 256; ++i) {
            const double v = -std::log2(std::sin(((i + 0.5) * kPi) / 512.0)) * 256.0;
            log_sin[i] = uint16_t(std::lround(v));
        }
        for (int i = 0; i < 256; ++i) {
            // Math.floor(0.5 + x), not Math.round: reproduces (int)(0.5+x) truncation (chip.ts).
            const double v = std::floor(0.5 + (std::pow(2.0, i / 256.0) - 1.0) * 2048.0);
            exp_tab[i] = uint16_t(v);
        }
        for (int r = 0; r < 4; ++r)
            for (int k = 0; k < 8; ++k) eg_inc[r][k] = int8_t(kEgInc[r][k]);
        for (int i = 0; i < 16; ++i) mult2[i] = uint8_t(kMult2[i]);
        for (int i = 0; i < 8; ++i) vibrato_steps[i] = int8_t(kVibratoSteps[i]);
    }
};
// A3-04A: a NAMESPACE-scope object, built once by the startup constructors,
// never a function-local static. The firmware compiles with
// -mdisable-hardware-atomics, so GCC cannot inline the "already built?"
// check of a function-local static and calls __cxa_guard_acquire -- a
// FreeRTOS mutex take + give -- on EVERY access. The synth reads these
// tables four times per sounding channel per chip sample (~1.7 million
// mutex round trips a second on the real corpus): that was A3-04's stutter
// (ALPHA3_AUDIO.md section 18.3). Nothing reads them during static
// initialization, so there is no initialization-order hazard.
const OplTables kTables;
const OplTables &tables() { return kTables; }

constexpr int32_t kSilentAtten = 0x1000; // "mute without branching": expo(SILENT) == 0
constexpr uint32_t kWaveNeg = 0x10000;

OPENU5_SYNTH_INLINE int32_t expo(int32_t att) {
    const int32_t a = att < 0 ? 0 : att > 0x1fff ? 0x1fff : att;
    const int32_t shift = a >> 8;
    if (shift >= 20) return 0;
    return (int32_t(tables().exp_tab[255 - (a & 0xff)]) + 2048) >> shift;
}

// A3-04B (ALPHA3_AUDIO.md section 19.7): the resampler's libm calls, exactly,
// without libm. On the device std::ceil / std::lround live in flash and run
// in software double arithmetic; these are integer operations.

/** std::ceil(x) for 0 < x < 2^32: the conversion truncates, and double(t) is exact. */
inline size_t ceil_positive(double x) {
    const size_t t = size_t(x);
    return double(t) < x ? t + 1 : t;
}

/**
 * std::lround(double(v) * 32767.0), for every float v below 2 in magnitude,
 * and a value at least 65536 in magnitude (which the caller's clamp turns
 * into what lround's would) beyond. double(v) * 32767.0 is exact -- a 24-bit
 * significand times a 15-bit integer fits a double's 53 bits -- so the
 * product is m * 32767 * 2^-s for v's significand m and scale s, and
 * lround's half-away-from-zero is a round-half-up of that magnitude.
 */
inline int32_t round_q15(float v) {
    uint32_t bits;
    std::memcpy(&bits, &v, sizeof bits);
    const bool negative = (bits >> 31) != 0;
    const uint32_t exponent = (bits >> 23) & 0xffu;
    if (exponent >= 128) return negative ? -65536 : 65536;      // |v| >= 2 (never reached: |v| <= 1)
    const uint32_t shift = 150u - exponent;                     // |v| = significand * 2^-shift
    if (shift >= 40) return 0;                                   // |v| * 32767 < 2^39 * 2^-40 = 1/2 (zeros, subnormals too)
    const uint64_t product = uint64_t((bits & 0x7fffffu) | 0x800000u) * 32767u;
    const int32_t magnitude = int32_t((product + (uint64_t(1) << (shift - 1))) >> shift);
    return negative ? -magnitude : magnitude;
}

struct WaveAtten {
    int32_t att;
    bool neg;
};

/**
 * chip.ts waveAtten, minus the packed-int GC-avoidance trick (unneeded in
 * C++: this is a stack value). A3-04B: an if-chain rather than A3-04A's
 * switch, case for case -- a switch this size becomes a jump table, which
 * the device keeps in flash .rodata (section 19.8). Same results for every
 * waveform: A3-04A's `default` served only 7, the one value above 6 that
 * write_reg() lets through.
 */
OPENU5_SYNTH_INLINE WaveAtten wave_atten(uint8_t wave, int32_t phase) {
    const int32_t quarter = phase & 0xff;
    const bool half = (phase & 0x200) != 0;
    const auto &t = tables();
    if (wave < 4) {
        const int32_t mirrored = (phase & 0x100) ? 255 - quarter : quarter;
        if (wave == 0) return {t.log_sin[mirrored], half};
        if (wave == 1) return half ? WaveAtten{kSilentAtten, false} : WaveAtten{t.log_sin[mirrored], false};
        if (wave == 2) return {t.log_sin[mirrored], false};
        return (phase & 0x100) ? WaveAtten{kSilentAtten, false} : WaveAtten{t.log_sin[quarter], false};
    }
    if (wave < 6) { // 4 and 5
        if (half) return {kSilentAtten, false};
        const int32_t d = (phase << 1) & 0x3ff;
        const int32_t q = d & 0xff;
        const int32_t idx = (d & 0x100) ? 255 - q : q;
        return {t.log_sin[idx], wave == 4 && (d & 0x200) != 0};
    }
    if (wave == 6) return {0, half};
    const int32_t v = phase & 0x1ff;
    return {(half ? 0x1ff - v : v) << 3, half};
}

constexpr uint8_t kEnvOff = 0, kEnvAttack = 1, kEnvDecay = 2, kEnvSustain = 3, kEnvRelease = 4;
constexpr int32_t kKslRom[16] = {0, 32, 40, 45, 48, 51, 53, 55, 56, 58, 59, 60, 61, 62, 63, 64};
constexpr uint8_t kKslShift[4] = {31, 1, 2, 0};
constexpr uint32_t kTremoloSteps = 52;
const uint32_t kTremoloPeriod = uint32_t(std::lround(double(kOplClockHz) / 3.7 / double(kTremoloSteps)));
const uint32_t kVibratoPeriod = uint32_t(std::lround(double(kOplClockHz) / 6.1 / 8.0));
constexpr int32_t kModScale = 1;
constexpr float kOutputScale = 1.0f / 16384.0f;
constexpr size_t kOpOffset9[9] = {0x00, 0x01, 0x02, 0x08, 0x09, 0x0a, 0x10, 0x11, 0x12};

struct RegOp {
    size_t channel;
    bool carrier;
    bool valid = false;
};
/** offset 0x00..0x15 -> (channel within its 9-wide array, carrier?) (chip.ts REG_TO_OP). */
RegOp reg_to_op(size_t offset) {
    for (size_t ch = 0; ch < 9; ++ch) {
        if (kOpOffset9[ch] == offset) return {ch, false, true};
        if (kOpOffset9[ch] + 3 == offset) return {ch, true, true};
    }
    return {0, false, false};
}

} // namespace

namespace test_only {
uint16_t opl_log_sin(int index) { return tables().log_sin[index & 0xff]; }
uint16_t opl_exp(int index) { return tables().exp_tab[index & 0xff]; }
int32_t opl_expo(int32_t att) { return expo(att); }
int32_t opl_round_q15(float v) { return round_q15(v); }
size_t opl_ceil_positive(double x) { return ceil_positive(x); }
} // namespace test_only

// ===========================================================================
// MilesOplBank (bank.ts parseMilesOplBank)
// ===========================================================================
bool MilesOplBank::load(const uint8_t *data, size_t size) {
    count_ = 0;
    if (!data) return false;
    constexpr size_t kIndexEntry = 6, kBlock = 14;
    size_t pos = 0;
    for (;;) {
        if (size < 2 || pos > size - 2) return false; // no 0xFFFF terminator
        const uint16_t sentinel = uint16_t(data[pos] | (data[pos + 1] << 8));
        if (sentinel == 0xffff) break;
        if (pos > size - kIndexEntry) return false;
        const uint8_t patch = data[pos];
        const uint8_t bank = data[pos + 1];
        const uint32_t offset = uint32_t(data[pos + 2]) | (uint32_t(data[pos + 3]) << 8) |
                                 (uint32_t(data[pos + 4]) << 16) | (uint32_t(data[pos + 5]) << 24);
        if (offset > size || size - offset < kBlock) return false;
        const uint16_t block_size = uint16_t(data[offset] | (data[offset + 1] << 8));
        if (block_size != kBlock) return false; // 4-op bank: rejected, not guessed (bank.ts)
        if (count_ >= kMaxOplTimbres) return false;
        OplTimbre &t = timbres_[count_++];
        t.bank = bank;
        t.patch = patch;
        t.fixed_note = data[offset + 2];
        auto read_op = [&](size_t off, OplOperatorDef &op) {
            op.am_vib_eg_ksr_mult = data[offset + off];
            op.ksl_tl = data[offset + off + 1];
            op.attack_decay = data[offset + off + 2];
            op.sustain_release = data[offset + off + 3];
            op.waveform = data[offset + off + 4];
        };
        read_op(3, t.modulator);
        t.feedback_connection = data[offset + 8];
        read_op(9, t.carrier);
        pos += kIndexEntry;
    }
    return count_ > 0;
}

const OplTimbre *MilesOplBank::get(uint8_t bank, uint8_t patch) const {
    for (size_t i = 0; i < count_; ++i)
        if (timbres_[i].bank == bank && timbres_[i].patch == patch) return &timbres_[i];
    return nullptr;
}

const OplTimbre &MilesOplBank::melodic(uint8_t program) const {
    const OplTimbre *found = get(kMilesBankMelodic, program & 0x7f);
    if (found) return *found;
    const OplTimbre *fallback = get(kMilesBankMelodic, 0);
    return fallback ? *fallback : timbres_[0];
}

const OplTimbre *MilesOplBank::percussion(uint8_t note) const { return get(kMilesBankPercussion, note & 0x7f); }

// ===========================================================================
// OplEmulator (chip.ts)
// ===========================================================================
OplEmulator::OplEmulator(OplChipKind kind)
    : kind_(kind), channel_count_(kind == OplChipKind::Opl3 ? 18 : 9) {}

void OplEmulator::Operator::update_ksl(uint16_t fnum, uint8_t block) {
    if (ksl == 0) {
        ksl_atten = 0;
        return;
    }
    const int32_t base = kKslRom[(fnum >> 6) & 0x0f] - ((7 - block) << 3);
    ksl_atten = base <= 0 ? 0 : (base >> kKslShift[ksl]) * 16;
}

void OplEmulator::Operator::key_on() {
    eg_state = kEnvAttack;
    phase = 0;
    prev1 = 0;
    prev2 = 0;
}

void OplEmulator::Operator::key_off() {
    if (eg_state != kEnvOff) eg_state = kEnvRelease;
}

uint8_t OplEmulator::Operator::rate_for(uint8_t reg, uint8_t key_code) const {
    if (reg == 0) return 0;
    const uint8_t add = ksr ? key_code : uint8_t(key_code >> 2);
    const int r = int(reg) * 4 + add;
    return r > 63 ? 63 : uint8_t(r);
}

// A3-04A's advance_envelope(), split in two (A3-04B, ALPHA3_AUDIO.md section
// 19.7). It ran for every sounding operator on every chip sample and, for
// all but the fastest rates, returned at its mask test: (eg_counter & mask)
// is non-zero for all but one sample in 2^shift. The part before that test
// depends only on the state, the rate registers and the key code, so it is
// now computed when one of those changes (refresh_envelope), and the
// per-sample loop makes the mask test inline; envelope_step() is the rest,
// unchanged.
void OplEmulator::Operator::refresh_envelope(uint8_t key_code) {
    env_rate = 0;
    env_shift = 0;
    env_mask = 0;
    if (eg_state == kEnvOff) return;
    const uint8_t reg = eg_state == kEnvAttack ? attack_rate
                         : eg_state == kEnvDecay ? decay_rate
                         : eg_state == kEnvSustain ? (eg_sustain ? 0 : release_rate)
                                                    : release_rate;
    const uint8_t rate = rate_for(reg, key_code);
    if (rate == 0) return;
    const uint8_t hi = rate >> 2;
    env_rate = rate;
    env_shift = uint8_t(hi < 13 ? 13 - hi : 0);
    env_mask = (1u << env_shift) - 1;
}

void OplEmulator::Operator::envelope_step(uint32_t eg_counter, uint8_t key_code) {
    const uint8_t hi = env_rate >> 2;
    const uint32_t scale = hi < 13 ? 1u : (1u << (hi - 13));
    const int32_t inc = int32_t(tables().eg_inc[env_rate & 3][(eg_counter >> env_shift) & 7]) * int32_t(scale);
    if (inc == 0) return;
    const uint8_t state = eg_state;
    if (eg_state == kEnvAttack) {
        eg_level += (~eg_level * inc) >> 3;
        if (eg_level <= 0) {
            eg_level = 0;
            eg_state = kEnvDecay;
        }
    } else {
        eg_level += inc;
        if (eg_level >= 511) {
            eg_level = 511;
            if (eg_state == kEnvRelease) eg_state = kEnvOff;
        } else if (eg_state == kEnvDecay && eg_level >= sustain_level) {
            eg_level = sustain_level;
            eg_state = kEnvSustain;
        }
    }
    if (eg_state != state) refresh_envelope(key_code); // the next rate applies from the next sample, as before
}

void OplEmulator::Channel::update_ksl() {
    mod.update_ksl(fnum, block);
    car.update_ksl(fnum, block);
}

OplEmulator::Operator *OplEmulator::op(size_t channel, bool carrier) {
    if (channel >= channel_count_) return nullptr;
    return carrier ? &channels_[channel].car : &channels_[channel].mod;
}

void OplEmulator::write_reg(uint16_t reg, uint8_t value) {
    const int array = reg >= 0x100 ? 1 : 0;
    const uint8_t r = uint8_t(reg & 0xff);
    const uint8_t v = value;
    if (array == 1 && kind_ != OplChipKind::Opl3) return;

    if (reg == 0x105) {
        opl3_enabled_ = (v & 1) != 0;
        return;
    }
    if (reg == 0x104 || r == 0x01 || r == 0x08 || r == 0x02 || r == 0x03) return;
    if (r == 0xbd) {
        tremolo_depth_ = (v & 0x80) != 0;
        vibrato_depth_ = (v & 0x40) != 0;
        refresh_vibrato(); // A3-04B: the depth scales every vibrato operator's phase step
        return; // rhythm mode: unused (the voice allocator never sets it)
    }

    // A3-04B: every write below ends by re-deriving its channel's cached
    // operator values (refresh_channel), whatever it changed -- one rule,
    // so no register can be missed. Writes are per MIDI event, not per sample.
    const size_t chBase = size_t(array) * 9;
    if (r >= 0x20 && r <= 0x35) {
        const RegOp m = reg_to_op(r - 0x20);
        if (!m.valid) return;
        Operator *o = op(chBase + m.channel, m.carrier);
        if (!o) return;
        o->am = (v & 0x80) != 0;
        o->vib = (v & 0x40) != 0;
        o->eg_sustain = (v & 0x20) != 0;
        o->ksr = (v & 0x10) != 0;
        o->mult = v & 0x0f;
        refresh_channel(channels_[chBase + m.channel]);
        return;
    }
    if (r >= 0x40 && r <= 0x55) {
        const RegOp m = reg_to_op(r - 0x40);
        if (!m.valid) return;
        if (chBase + m.channel >= channel_count_) return;
        Channel &ch = channels_[chBase + m.channel];
        Operator &o = m.carrier ? ch.car : ch.mod;
        o.ksl = v >> 6;
        o.tl = v & 0x3f;
        o.update_ksl(ch.fnum, ch.block);
        refresh_channel(ch);
        return;
    }
    if (r >= 0x60 && r <= 0x75) {
        const RegOp m = reg_to_op(r - 0x60);
        if (!m.valid) return;
        Operator *o = op(chBase + m.channel, m.carrier);
        if (!o) return;
        o->attack_rate = v >> 4;
        o->decay_rate = v & 0x0f;
        refresh_channel(channels_[chBase + m.channel]);
        return;
    }
    if (r >= 0x80 && r <= 0x95) {
        const RegOp m = reg_to_op(r - 0x80);
        if (!m.valid) return;
        Operator *o = op(chBase + m.channel, m.carrier);
        if (!o) return;
        const uint8_t sl = v >> 4;
        o->sustain_level = uint16_t((sl == 15 ? 31 : sl) << 4);
        o->release_rate = v & 0x0f;
        refresh_channel(channels_[chBase + m.channel]);
        return;
    }
    if (r >= 0xe0 && r <= 0xf5) {
        const RegOp m = reg_to_op(r - 0xe0);
        if (!m.valid) return;
        Operator *o = op(chBase + m.channel, m.carrier);
        if (!o) return;
        o->waveform = (kind_ == OplChipKind::Opl3 && opl3_enabled_) ? (v & 0x07) : (v & 0x03);
        refresh_channel(channels_[chBase + m.channel]);
        return;
    }
    if (r >= 0xa0 && r <= 0xa8) {
        if (chBase + (r - 0xa0) >= channel_count_) return;
        Channel &ch = channels_[chBase + (r - 0xa0)];
        ch.fnum = uint16_t((ch.fnum & 0x300) | v);
        ch.update_ksl();
        refresh_channel(ch);
        return;
    }
    if (r >= 0xb0 && r <= 0xb8) {
        if (chBase + (r - 0xb0) >= channel_count_) return;
        Channel &ch = channels_[chBase + (r - 0xb0)];
        ch.fnum = uint16_t((ch.fnum & 0xff) | ((v & 0x03) << 8));
        ch.block = uint8_t((v >> 2) & 0x07);
        ch.update_ksl();
        const bool on = (v & 0x20) != 0;
        if (on && !ch.keyed) {
            ch.mod.key_on();
            ch.car.key_on();
        } else if (!on && ch.keyed) {
            ch.mod.key_off();
            ch.car.key_off();
        }
        ch.keyed = on;
        refresh_channel(ch);
        return;
    }
    if (r >= 0xc0 && r <= 0xc8) {
        if (chBase + (r - 0xc0) >= channel_count_) return;
        Channel &ch = channels_[chBase + (r - 0xc0)];
        ch.feedback = uint8_t((v >> 1) & 0x07);
        ch.additive = (v & 0x01) != 0;
        if (kind_ == OplChipKind::Opl3 && opl3_enabled_) {
            ch.left = (v & 0x20) != 0;
            ch.right = (v & 0x10) != 0;
        } else {
            ch.left = true;
            ch.right = true;
        }
    }
}

uint32_t OplEmulator::phase_inc(const Channel &ch, const Operator &o) const {
    int32_t fnum = ch.fnum;
    if (o.vib) {
        const int32_t unit = vibrato_depth_ ? (fnum >> 8) : (fnum >> 9);
        fnum += int32_t(tables().vibrato_steps[vibrato_pos_]) * unit;
        if (fnum < 0) fnum = 0;
    }
    // A3-04B: 32-bit, not A3-04A's int64 (a shift and multiply the ESP32-S3
    // does in ~20 instructions). Same value: fnum <= 1023 + 2 * 3 after
    // vibrato, block <= 7 and kMult2 <= 30, so the product stays below 2^22.
    return ((uint32_t(fnum) << ch.block) * uint32_t(tables().mult2[o.mult])) >> 1;
}

void OplEmulator::refresh_channel(Channel &ch) {
    const uint8_t key_code = ch.key_code();
    for (Operator *o : {&ch.mod, &ch.car}) {
        o->inc = phase_inc(ch, *o);
        o->static_atten = (int32_t(o->tl) << 5) + o->ksl_atten;
        o->refresh_envelope(key_code);
    }
}

void OplEmulator::refresh_vibrato() {
    for (size_t c = 0; c < channel_count_; ++c) {
        Channel &ch = channels_[c];
        if (ch.mod.vib) ch.mod.inc = phase_inc(ch, ch.mod);
        if (ch.car.vib) ch.car.inc = phase_inc(ch, ch.car);
    }
}

OPENU5_SYNTH_INLINE void OplEmulator::advance_clocks() {
    ++eg_counter_;
    if (++tremolo_counter_ >= kTremoloPeriod) {
        tremolo_counter_ = 0;
        tremolo_pos_ = (tremolo_pos_ + 1) % kTremoloSteps;
    }
    if (++vibrato_counter_ >= kVibratoPeriod) {
        vibrato_counter_ = 0;
        vibrato_pos_ = (vibrato_pos_ + 1) & 7;
        refresh_vibrato(); // A3-04B: once per ~1,019 chip samples, before this sample's phase steps
    }
}

int32_t OplEmulator::tremolo_atten() const {
    const uint32_t half = kTremoloSteps / 2;
    const uint32_t tri = tremolo_pos_ < half ? tremolo_pos_ : (kTremoloSteps - tremolo_pos_);
    return int32_t((tremolo_depth_ ? tri : (tri >> 2)) * 8);
}

// A3-04A's Operator::sample(), with the phase step read from the cache and
// one early-out added (A3-04B, section 19.7): a static attenuation at or
// above kSilentFloor makes expo() 0 whatever the waveform adds, so the
// waveform and exp lookups are skipped. The waveform attenuation and the
// tremolo are never negative, so they can only add to it.
OPENU5_SYNTH_INLINE int32_t OplEmulator::Operator::sample(int32_t modulation, int32_t tremolo) {
    phase = phase + inc; // uint32_t: wraps exactly like the TS >>> 0
    if (eg_state == kEnvOff) return 0;
    const int32_t base = (eg_level << 3) + static_atten;
    if (base >= kSilentFloor) return 0;
    const int32_t idx = (int32_t(phase >> 10) + modulation) & 0x3ff;
    const WaveAtten w = wave_atten(waveform, idx);
    const int32_t mag = expo(w.att + base + (am ? tremolo : 0));
    return w.neg ? -mag : mag;
}

void OplEmulator::generate(float *left, float *right, size_t count, size_t offset) {
    for (size_t i = offset; i < offset + count; ++i) {
        advance_clocks();
        const int32_t trem = tremolo_atten();
        const uint32_t eg = eg_counter_;
        float l = 0, r = 0;
        for (size_t c = 0; c < channel_count_; ++c) {
            Channel &ch = channels_[c];
            if (ch.mod.eg_state == kEnvOff && ch.car.eg_state == kEnvOff) continue;
            // Both envelopes first, then both operators (A3-04A's order). The
            // inline test is advance_envelope()'s own early-outs: Off or a
            // zero rate (env_rate 0), then the rate's counter mask.
            if (ch.mod.env_rate && (eg & ch.mod.env_mask) == 0) ch.mod.envelope_step(eg, ch.key_code());
            if (ch.car.env_rate && (eg & ch.car.env_mask) == 0) ch.car.envelope_step(eg, ch.key_code());

            const int32_t fb = ch.feedback == 0 ? 0 : (ch.mod.prev1 + ch.mod.prev2) >> (9 - ch.feedback);
            const int32_t modOut = ch.mod.sample(fb, trem);
            ch.mod.prev2 = ch.mod.prev1;
            ch.mod.prev1 = modOut;

            const int32_t carOut = ch.car.sample(ch.additive ? 0 : modOut * kModScale, trem);
            const int32_t out = ch.additive ? modOut + carOut : carOut;
            if (ch.left) l += float(out);
            if (right && ch.right) r += float(out);
        }
        const float lf = l * kOutputScale;
        left[i] = lf > 1.0f ? 1.0f : lf < -1.0f ? -1.0f : lf;
        if (right) {
            const float rf = r * kOutputScale;
            right[i] = rf > 1.0f ? 1.0f : rf < -1.0f ? -1.0f : rf;
        }
    }
}

bool OplEmulator::is_silent() const {
    for (size_t c = 0; c < channel_count_; ++c)
        if (channels_[c].mod.eg_state != kEnvOff || channels_[c].car.eg_state != kEnvOff) return false;
    return true;
}

size_t OplEmulator::sounding_channels() const {
    size_t n = 0;
    for (size_t c = 0; c < channel_count_; ++c)
        if (channels_[c].mod.eg_state != kEnvOff || channels_[c].car.eg_state != kEnvOff) ++n;
    return n;
}

// ===========================================================================
// note/volume helpers (voices.ts noteToFnumBlock / scaleTl)
// ===========================================================================
FnumBlock note_to_fnum_block(float note_with_bend) {
    const double freq = 440.0 * std::pow(2.0, (double(note_with_bend) - 69.0) / 12.0);
    int block = 0;
    double fnum = (freq * double(0x100000)) / double(kOplClockHz);
    while (fnum > 1023.0 && block < 7) {
        ++block;
        fnum /= 2.0;
    }
    if (fnum < 0.0) fnum = 0.0;
    if (fnum > 1023.0) fnum = 1023.0;
    return {uint16_t(std::lround(fnum)), uint8_t(block)};
}

uint8_t scale_tl(uint8_t ksl_tl, float vol01) {
    const uint8_t base = ksl_tl & 0x3f;
    const float v = vol01 < 0.0f ? 0.0f : vol01 > 1.0f ? 1.0f : vol01;
    int tl = int(std::lround(double(base) + (63.0 - base) * (1.0 - v)));
    if (tl < 0) tl = 0;
    if (tl > 63) tl = 63;
    return uint8_t((ksl_tl & 0xc0) | tl);
}

// ===========================================================================
// OplVoiceAllocator (voices.ts)
// ===========================================================================
namespace {
constexpr uint16_t kRegTestWaveEnable = 0x01, kRegCsmNotesel = 0x08, kRegRhythm = 0xbd;
constexpr uint16_t kRegOpl3FourOp = 0x104, kRegOpl3Enable = 0x105;
constexpr uint8_t kStereoRight = 0x10, kStereoLeft = 0x20, kStereoBoth = kStereoLeft | kStereoRight;
constexpr uint8_t kKeyOn = 0x20;
} // namespace

OplVoiceAllocator::OplVoiceAllocator(const MilesOplBank &bank, OplChipKind chip, void *sink_ctx, RegSink sink)
    : bank_(bank), chip_(chip), voice_count_(chip == OplChipKind::Opl3 ? 18 : 9), sink_ctx_(sink_ctx),
      sink_(sink) {}

size_t OplVoiceAllocator::op_offset(size_t voice) const { return kOpOffset9[slot(voice)]; }

void OplVoiceAllocator::reset() {
    if (chip_ == OplChipKind::Opl3) {
        emit(kRegOpl3Enable, 0x01);
        emit(kRegOpl3FourOp, 0x00);
    }
    emit(kRegTestWaveEnable, 0x20); // wave-select enable (OPL2 needs it; harmless on OPL3)
    emit(kRegCsmNotesel, 0x00);
    emit(kRegRhythm, 0x00);
    if (chip_ == OplChipKind::Opl3) emit(0x100 + kRegRhythm, 0x00);
    for (size_t i = 0; i < voice_count_; ++i) {
        const size_t base = array_base(i);
        const size_t off = op_offset(i);
        emit(uint16_t(base + 0xb0 + slot(i)), 0x00);
        emit(uint16_t(base + 0x40 + off), 0x3f);
        emit(uint16_t(base + 0x40 + off + 3), 0x3f);
        voices_[i] = Voice{};
    }
    for (auto &c : channels_) c = ChannelState{};
    age_ = 0;
}

float OplVoiceAllocator::volume_of(const ChannelState &ch, uint8_t velocity) const {
    return (float(velocity) / 127.0f) * (float(ch.volume) / 127.0f) * (float(ch.expression) / 127.0f);
}

uint8_t OplVoiceAllocator::stereo_for(const ChannelState &ch, const OplTimbre &t) const {
    if (chip_ != OplChipKind::Opl3) return t.feedback_connection & 0x0f;
    const uint8_t pan = ch.pan <= 42 ? kStereoLeft : ch.pan >= 85 ? kStereoRight : kStereoBoth;
    return uint8_t((t.feedback_connection & 0x0f) | pan);
}

void OplVoiceAllocator::write_operator(size_t base, size_t off, const OplOperatorDef &op, uint8_t ksl_tl) {
    emit(uint16_t(base + 0x20 + off), op.am_vib_eg_ksr_mult);
    emit(uint16_t(base + 0x40 + off), ksl_tl);
    emit(uint16_t(base + 0x60 + off), op.attack_decay);
    emit(uint16_t(base + 0x80 + off), op.sustain_release);
    emit(uint16_t(base + 0xe0 + off), op.waveform);
}

void OplVoiceAllocator::write_timbre(size_t voice, float vol01) {
    Voice &v = voices_[voice];
    if (!v.timbre) return;
    const OplTimbre &t = *v.timbre;
    const size_t base = array_base(voice);
    const size_t off = op_offset(voice);
    const bool additive = (t.feedback_connection & 0x01) != 0;
    write_operator(base, off, t.modulator, additive ? scale_tl(t.modulator.ksl_tl, vol01) : t.modulator.ksl_tl);
    write_operator(base, off + 3, t.carrier, scale_tl(t.carrier.ksl_tl, vol01));
    emit(uint16_t(base + 0xc0 + slot(voice)), stereo_for(channels_[size_t(v.midi_ch)], t));
}

void OplVoiceAllocator::write_pitch(size_t voice, bool key_on) {
    Voice &v = voices_[voice];
    const size_t base = array_base(voice);
    const size_t sl = slot(voice);
    emit(uint16_t(base + 0xa0 + sl), uint8_t(v.fnum & 0xff));
    emit(uint16_t(base + 0xb0 + sl),
         uint8_t(((v.fnum >> 8) & 0x03) | ((v.block & 0x07) << 2) | (key_on ? kKeyOn : 0)));
}

size_t OplVoiceAllocator::pick_voice(uint8_t midi_ch, uint8_t note) const {
    size_t free_v = SIZE_MAX, held_v = SIZE_MAX, sounding_v = SIZE_MAX;
    uint32_t free_age = UINT32_MAX, held_age = UINT32_MAX, sounding_age = UINT32_MAX;
    for (size_t i = 0; i < voice_count_; ++i) {
        const Voice &v = voices_[i];
        if (v.keyed && v.midi_ch == int8_t(midi_ch) && v.note == int16_t(note)) return i;
        if (!v.keyed && !v.sustained) {
            if (v.age_off < free_age) {
                free_age = v.age_off;
                free_v = i;
            }
        } else if (v.sustained) {
            if (v.age_on < held_age) {
                held_age = v.age_on;
                held_v = i;
            }
        } else if (v.age_on < sounding_age) {
            sounding_age = v.age_on;
            sounding_v = i;
        }
    }
    if (free_v != SIZE_MAX) return free_v;
    if (held_v != SIZE_MAX) return held_v;
    return sounding_v != SIZE_MAX ? sounding_v : 0;
}

void OplVoiceAllocator::release_voice(size_t voice) {
    Voice &v = voices_[voice];
    v.keyed = false;
    v.sustained = false;
    v.age_off = ++age_;
    write_pitch(voice, false);
}

void OplVoiceAllocator::note_on(uint8_t channel, uint8_t note, uint8_t velocity) {
    if (velocity == 0) {
        note_off(channel, note);
        return;
    }
    const uint8_t c = channel & 0x0f;
    ChannelState &ch = channels_[c];
    const OplTimbre *timbre = c == kDrumChannel ? bank_.percussion(note) : &bank_.melodic(ch.program);
    if (!timbre) return; // percussion the bank does not carry: silent, not a guess

    const size_t voice = pick_voice(c, note);
    Voice &v = voices_[voice];
    if (v.keyed || v.sustained) {
        // Force the envelope to restart: a stolen voice that keeps its old
        // phase clicks and loses its attack (voices.ts noteOn comment).
        emit(uint16_t(array_base(voice) + 0xb0 + slot(voice)), 0x00);
    }
    v.midi_ch = int8_t(c);
    v.note = int16_t(note);
    v.played_note = int16_t(timbre->fixed_note > 0 ? timbre->fixed_note : note);
    v.timbre = timbre;
    v.velocity = velocity;
    v.keyed = true;
    v.sustained = false;
    v.age_on = ++age_;
    const FnumBlock fb = note_to_fnum_block(float(v.played_note) + ch.bend_semitones);
    v.fnum = fb.fnum;
    v.block = fb.block;
    write_timbre(voice, volume_of(ch, velocity));
    write_pitch(voice, true);
}

void OplVoiceAllocator::note_off(uint8_t channel, uint8_t note) {
    const uint8_t c = channel & 0x0f;
    ChannelState &ch = channels_[c];
    for (size_t i = 0; i < voice_count_; ++i) {
        Voice &v = voices_[i];
        if (!v.keyed || v.midi_ch != int8_t(c) || v.note != int16_t(note)) continue;
        if (ch.sustain) {
            v.sustained = true;
            continue;
        }
        release_voice(i);
    }
}

void OplVoiceAllocator::program_change(uint8_t channel, uint8_t program) {
    channels_[channel & 0x0f].program = program & 0x7f;
    // MIDI program change affects notes struck AFTER it, never voices already sounding.
}

void OplVoiceAllocator::pitch_bend(uint8_t channel, uint16_t value14) {
    const uint8_t c = channel & 0x0f;
    channels_[c].bend_semitones = ((float(value14) - 8192.0f) / 8192.0f) * float(kBendRangeSemitones);
    refresh_pitch(c);
}

void OplVoiceAllocator::refresh_pitch(uint8_t channel) {
    ChannelState &ch = channels_[channel];
    for (size_t i = 0; i < voice_count_; ++i) {
        Voice &v = voices_[i];
        if (v.midi_ch != int8_t(channel) || (!v.keyed && !v.sustained)) continue;
        const FnumBlock fb = note_to_fnum_block(float(v.played_note) + ch.bend_semitones);
        v.fnum = fb.fnum;
        v.block = fb.block;
        write_pitch(i, true);
    }
}

void OplVoiceAllocator::refresh_volume(uint8_t channel) {
    ChannelState &ch = channels_[channel];
    for (size_t i = 0; i < voice_count_; ++i) {
        Voice &v = voices_[i];
        if (v.midi_ch != int8_t(channel) || (!v.keyed && !v.sustained) || !v.timbre) continue;
        write_timbre(i, volume_of(ch, v.velocity));
    }
}

void OplVoiceAllocator::all_notes_off_impl(int channel) {
    for (size_t i = 0; i < voice_count_; ++i) {
        Voice &v = voices_[i];
        if (channel >= 0 && v.midi_ch != int8_t(channel)) continue;
        if (v.keyed || v.sustained) release_voice(i);
    }
}

void OplVoiceAllocator::all_notes_off(int channel) { all_notes_off_impl(channel); }

void OplVoiceAllocator::control_change(uint8_t channel, uint8_t controller, uint8_t value) {
    const uint8_t c = channel & 0x0f;
    ChannelState &ch = channels_[c];
    switch (controller) {
    case 7:
        ch.volume = value & 0x7f;
        refresh_volume(c);
        break;
    case 11:
        ch.expression = value & 0x7f;
        refresh_volume(c);
        break;
    case 10:
        ch.pan = value & 0x7f;
        refresh_volume(c); // rewrites 0xC0 with the new stereo bits
        break;
    case 64: {
        const bool pressed = value >= 64;
        if (ch.sustain != pressed) {
            ch.sustain = pressed;
            if (!pressed) {
                for (size_t i = 0; i < voice_count_; ++i) {
                    Voice &v = voices_[i];
                    if (v.midi_ch == int8_t(c) && v.sustained) release_voice(i);
                }
            }
        }
        break;
    }
    case 120:
    case 123:
        all_notes_off_impl(int(c));
        break;
    case 121: // reset all controllers: defaults, but the program and live notes stand (GM)
        ch.volume = 100;
        ch.expression = 127;
        ch.pan = 64;
        ch.bend_semitones = 0;
        if (ch.sustain) {
            ch.sustain = false;
            for (size_t i = 0; i < voice_count_; ++i) {
                Voice &v = voices_[i];
                if (v.midi_ch == int8_t(c) && v.sustained) release_voice(i);
            }
        }
        refresh_volume(c);
        refresh_pitch(c);
        break;
    default:
        // Accepted and ignored on purpose (voices.ts): 1/32/91/93/119 are in
        // the corpus and the OPL has nothing to represent them with.
        break;
    }
}

size_t OplVoiceAllocator::active_voices() const {
    size_t n = 0;
    for (size_t i = 0; i < voice_count_; ++i)
        if (voices_[i].keyed || voices_[i].sustained) ++n;
    return n;
}

// ===========================================================================
// XMI event parsing (xmi2midi.ts evntToMidi, minus the MIDI re-encode)
// ===========================================================================
namespace {
uint32_t fourcc_u32be(const uint8_t *d) {
    return (uint32_t(d[0]) << 24) | (uint32_t(d[1]) << 16) | (uint32_t(d[2]) << 8) | uint32_t(d[3]);
}
bool id4(const uint8_t *d, const char *id) { return std::memcmp(d, id, 4) == 0; }

/** The first EVNT chunk's [body, body+length) inside an XMI IFF stream. */
bool find_first_evnt(const uint8_t *d, size_t n, size_t &body, size_t &length) {
    struct Frame {
        size_t at, end;
    };
    Frame stack[8];
    int top = 0;
    stack[top++] = {0, n};
    while (top > 0) {
        Frame f = stack[--top];
        size_t pos = f.at;
        while (pos + 8 <= f.end) {
            const size_t chunk_body = pos + 8;
            const uint32_t len = fourcc_u32be(d + pos + 4);
            if (chunk_body + len > f.end) return false;
            if (id4(d + pos, "FORM") || id4(d + pos, "CAT ")) {
                if (len < 4 || top >= 8) return false;
                stack[top++] = {chunk_body + 4, chunk_body + len};
            } else if (id4(d + pos, "EVNT")) {
                body = chunk_body;
                length = len;
                return true;
            }
            pos = chunk_body + len + (len & 1);
        }
    }
    return false;
}
} // namespace

bool parse_xmi_events(const uint8_t *data, size_t size, MusicTrack &out) {
    out.events.clear();
    out.end_tick = 0;
    size_t body = 0, length = 0;
    if (!find_first_evnt(data, size, body, length)) return false;
    const uint8_t *evnt = data + body;

    struct Raw {
        uint32_t tick;
        uint32_t order;
        MusicEvent ev;
    };
    std::vector<Raw> raw;
    raw.reserve(length / 3 + 8);

    uint32_t tick = 0;
    size_t pos = 0;
    uint32_t order = 0;
    auto read_vlq = [&]() -> uint32_t {
        uint32_t v = 0;
        for (;;) {
            if (pos >= length) return v;
            const uint8_t b = evnt[pos++];
            v = (v << 7) | (b & 0x7f);
            if ((b & 0x80) == 0) return v;
        }
    };

    while (pos < length) {
        const uint8_t b = evnt[pos];
        if (b < 0x80) {
            tick += b;
            ++pos;
            continue;
        }
        ++pos;
        if (b == 0xff) {
            if (pos >= length) return false;
            const uint8_t type = evnt[pos++];
            const uint32_t len = read_vlq();
            if (pos + len > length) return false;
            pos += len;
            if (type == 0x2f) {
                raw.push_back({tick, order++, MusicEvent{tick, 0xff, 0, 0}});
                break;
            }
            // 0x51 (tempo) and every other meta: no register writes depend on
            // them (kXmiTicksPerSecond is already that fixed tempo).
            continue;
        }
        const uint8_t status_hi = b & 0xf0;
        const uint8_t channel = b & 0x0f;
        if (status_hi == 0x90) {
            if (pos + 2 > length) return false;
            const uint8_t note = evnt[pos++];
            const uint8_t vel = evnt[pos++];
            const uint32_t duration = read_vlq();
            raw.push_back({tick, order++, MusicEvent{tick, uint8_t(0x90 | channel), note, vel}});
            raw.push_back({tick + duration, order++, MusicEvent{tick + duration, uint8_t(0x90 | channel), note, 0}});
        } else if (status_hi == 0xc0 || status_hi == 0xd0) {
            if (pos + 1 > length) return false;
            const uint8_t d1 = evnt[pos++];
            raw.push_back({tick, order++, MusicEvent{tick, b, d1, 0}});
        } else if (status_hi == 0x80 || status_hi == 0xa0 || status_hi == 0xb0 || status_hi == 0xe0) {
            if (pos + 2 > length) return false;
            const uint8_t d1 = evnt[pos++];
            const uint8_t d2 = evnt[pos++];
            raw.push_back({tick, order++, MusicEvent{tick, b, d1, d2}});
        } else {
            return false; // unknown status byte: reject, do not guess
        }
    }

    std::stable_sort(raw.begin(), raw.end(), [](const Raw &a, const Raw &b) { return a.tick < b.tick; });
    out.events.reserve(raw.size());
    for (const auto &r : raw) out.events.push_back(r.ev);
    out.end_tick = out.events.empty() ? 0 : out.events.back().tick;
    return true;
}

// ===========================================================================
// MusicSongPlayer (sequencer.ts OplSongPlayer)
// ===========================================================================
void MusicSongPlayer::sink_trampoline(void *ctx, uint16_t reg, uint8_t value) {
    static_cast<MusicSongPlayer *>(ctx)->chip_.write_reg(reg, value);
}

void MusicSongPlayer::start(const MusicTrack &track, const MilesOplBank &bank, OplChipKind chip, bool loop) {
    track_ = &track;
    bank_ = &bank;
    loop_ = loop;
    mono_ = chip == OplChipKind::Opl2;
    // A3-04A: rebuilt IN PLACE. `chip_ = OplEmulator(chip)` built a ~1.7 KB
    // temporary on the audio task's stack at every song switch; the emulator
    // is trivially destructible, so re-constructing it where it lives is the
    // same state with no temporary.
    chip_.~OplEmulator();
    new (&chip_) OplEmulator(chip);
    alloc_.emplace(bank, chip, this, &MusicSongPlayer::sink_trampoline);
    alloc_->reset();
    event_index_ = 0;
    song_sample_ = 0.0;
    ended_ = false;
    active_ = true;
    have_ = 0;
    read_pos_ = 0.0;
    last_output_rate_ = 0;
    ratio_ = 1.0;

    const double tick_to_sample = double(kOplClockHz) / double(kXmiTicksPerSecond);
    const double last = double(track.end_tick) * tick_to_sample;
    loop_sample_ = last > 1.0 ? last : 1.0; // a zero-length track cannot loop tighter than 1 sample
    end_sample_ = last + double(kOplClockHz) * 3.0; // 3 s tail for a non-looping track's release
}

void MusicSongPlayer::stop() {
    active_ = false;
    ended_ = false;
    alloc_.reset();
    track_ = nullptr;
    bank_ = nullptr;
}

void MusicSongPlayer::dispatch(const MusicEvent &e) {
    if (e.status == 0xff) return; // metas: nothing here depends on them (see parse_xmi_events)
    const uint8_t ch = e.status & 0x0f;
    switch (e.status & 0xf0) {
    case 0x90: alloc_->note_on(ch, e.d1, e.d2); return;
    case 0x80: alloc_->note_off(ch, e.d1); return;
    case 0xb0: alloc_->control_change(ch, e.d1, e.d2); return;
    case 0xc0: alloc_->program_change(ch, e.d1); return;
    case 0xe0: alloc_->pitch_bend(ch, uint16_t(e.d1 | (e.d2 << 7))); return;
    default: return; // aftertouch/channel pressure: the OPL cannot represent them
    }
}

void MusicSongPlayer::fill_chip(float *l, float *r, size_t n, size_t base) {
    const double tick_to_sample = double(kOplClockHz) / double(kXmiTicksPerSecond);
    const auto &events = track_->events;
    size_t done = 0;
    while (done < n) {
        while (event_index_ < events.size() && double(events[event_index_].tick) * tick_to_sample <= song_sample_) {
            dispatch(events[event_index_]);
            ++event_index_;
        }
        size_t budget = n - done;
        if (event_index_ < events.size()) {
            // Positive: the loop above consumed every event at or before song_sample_.
            const double until_d = double(events[event_index_].tick) * tick_to_sample - song_sample_;
            size_t until = ceil_positive(until_d);
            if (until < 1) until = 1;
            if (until < budget) budget = until;
        } else if (loop_) {
            // Seamless loop: rebobina without an allNotesOff. Releasing
            // voices from the previous pass keep sounding over the new
            // pass's attack, and pickVoice prefers unkeyed voices anyway
            // (sequencer.ts fillChip's loop branch, verbatim rationale).
            event_index_ = 0;
            song_sample_ = 0.0;
            continue;
        } else if (song_sample_ >= end_sample_) {
            ended_ = true;
            std::fill(l + base + done, l + base + n, 0.0f);
            if (r) std::fill(r + base + done, r + base + n, 0.0f);
            return;
        }
        chip_.generate(l, r, budget, base + done);
        song_sample_ += double(budget);
        done += budget;
    }
}

void MusicSongPlayer::ensure_capacity(size_t frames) {
    if (frames <= chip_capacity_) return;
    auto l = std::make_unique<float[]>(frames);
    auto r = std::make_unique<float[]>(frames);
    if (have_ > 0) {
        std::copy(chip_l_.get(), chip_l_.get() + have_, l.get());
        std::copy(chip_r_.get(), chip_r_.get() + have_, r.get());
    }
    chip_l_ = std::move(l);
    chip_r_ = std::move(r);
    chip_capacity_ = frames;
}

void MusicSongPlayer::render(int16_t *mono_out, size_t frames, uint32_t output_rate_hz, uint16_t gain_q15) {
    if (frames == 0) return;
    if (!active_) {
        std::fill(mono_out, mono_out + frames, int16_t(0));
        return;
    }
    if (output_rate_hz != last_output_rate_) {
        ratio_ = double(kOplClockHz) / double(output_rate_hz);
        last_output_rate_ = output_rate_hz;
    }

    // A3-04B: size_t(read_pos_) IS floor(read_pos_) -- it is never negative
    // (it starts at 0, grows by ratio_ and loses at most its own floor) -- and
    // needs no libm call (section 19.7). Same for i0 below.
    const size_t drop = size_t(read_pos_);
    if (drop > 0) {
        const size_t clamped = drop < have_ ? drop : have_;
        std::copy(chip_l_.get() + clamped, chip_l_.get() + have_, chip_l_.get());
        if (!mono_) std::copy(chip_r_.get() + clamped, chip_r_.get() + have_, chip_r_.get());
        have_ -= clamped;
        read_pos_ -= double(clamped);
    }

    // The capacity request uses a STABLE upper bound (read_pos_ replaced by
    // its supremum, 1.0, since drop above always leaves it in [0,1)) rather
    // than the exact read_pos_: with the exact value, the +/-1 sample jitter
    // from its fractional drift could ask for one sample more than last
    // time on an unlucky call and force a regrowth -- rare, but a hole in
    // the "never allocates once started" guarantee. A fixed (frames,
    // output_rate_hz) pair now always asks for the same capacity, so
    // ensure_capacity grows exactly once and never again.
    const size_t needed = ceil_positive(1.0 + ratio_ * double(frames - 1)) + 2;
    ensure_capacity(needed);
    if (needed > have_) {
        const size_t missing = needed - have_;
        fill_chip(chip_l_.get(), mono_ ? nullptr : chip_r_.get(), missing, have_);
        have_ += missing;
    }

    for (size_t i = 0; i < frames; ++i) {
        const size_t i0 = size_t(read_pos_);
        const size_t i1 = i0 + 1 < have_ ? i0 + 1 : i0;
        const float frac = float(read_pos_ - double(i0));
        const float a = chip_l_[i0], b = chip_l_[i1];
        const float left_s = a + (b - a) * frac;
        // OPL2: right == left, and (x + x) * 0.5f == x exactly (a doubling
        // and a halving of a float are both exact), so the left half IS
        // A3-04A's mono sample.
        float mono = left_s;
        if (!mono_) {
            const float c = chip_r_[i0], d = chip_r_[i1];
            const float right_s = c + (d - c) * frac;
            mono = (left_s + right_s) * 0.5f;
        }
        int32_t sample = round_q15(mono);
        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;
        mono_out[i] = apply_gain_q15(int16_t(sample), gain_q15);
        read_pos_ += ratio_;
    }
}

} // namespace openu5
