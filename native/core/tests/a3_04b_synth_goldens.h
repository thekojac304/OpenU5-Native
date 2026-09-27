// Alpha 3 A3-04B -- the synth's bit-exactness oracle (ALPHA3_AUDIO.md
// section 19). Every fingerprint below was computed from the A3-04A synth
// (commit e48abb8d, before any A3-04B change) by these same functions; the
// A3-04B per-operator fast paths must reproduce every one of them exactly.
//
// Header-only so the ad-hoc golden printer and the ctest share one
// definition of "the fingerprint".
#pragma once

#include "openu5/audio.h"
#include "openu5/audio_pack.h"
#include "openu5/music_synth.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace a3_04b {

struct Fnv {
    uint64_t h = 1469598103934665603ull;
    void add16(uint16_t v) {
        h ^= v;
        h *= 1099511628211ull;
    }
    void add32(uint32_t v) {
        add16(uint16_t(v));
        add16(uint16_t(v >> 16));
    }
    void addf(float f) {
        uint32_t bits;
        std::memcpy(&bits, &f, sizeof bits);
        add32(bits);
    }
};

/**
 * A3-04's own L6 fingerprint: every song once, no loop, 256-frame chunks,
 * 16 kHz, unity gain. `peak` (optional) receives the corpus's largest |sample|.
 */
inline uint64_t corpus_l6(const openu5::AudioPackPayload &payload, const openu5::MilesOplBank &bank,
                          int32_t *peak = nullptr) {
    using namespace openu5;
    uint64_t combined = 1469598103934665603ull;
    for (int s = 0; s < 16; ++s) {
        MusicTrack track;
        if (!parse_xmi_events(payload.song[s], payload.song_length[s], track)) return 0;
        const double seconds = double(track.end_tick) / double(kXmiTicksPerSecond);
        MusicSongPlayer player;
        player.start(track, bank, OplChipKind::Opl2, /*loop=*/false);
        std::vector<int16_t> buf(256);
        const size_t budget = size_t((seconds + 4.0) * 16000.0);
        for (size_t rendered = 0; rendered < budget && !player.ended(); rendered += buf.size()) {
            player.render(buf.data(), buf.size(), 16000, kUnityGainQ15);
            for (int16_t v : buf) {
                combined ^= uint64_t(uint16_t(v));
                combined *= 1099511628211ull;
                if (peak && (v < 0 ? -int32_t(v) : int32_t(v)) > *peak) *peak = v < 0 ? -int32_t(v) : int32_t(v);
            }
        }
    }
    return combined;
}

/**
 * The device's geometry: every song looping for `seconds`, rendered in
 * `chunk`-frame blocks at `rate` Hz and Music Volume 80 % (what the device's
 * pump asks of MusicSongPlayer), through `chip`.
 */
inline uint64_t corpus_stream(const openu5::AudioPackPayload &payload, const openu5::MilesOplBank &bank,
                              openu5::OplChipKind chip, size_t chunk, uint32_t rate, double seconds) {
    using namespace openu5;
    Fnv f;
    std::vector<int16_t> buf(chunk);
    for (int s = 0; s < 16; ++s) {
        MusicTrack track;
        if (!parse_xmi_events(payload.song[s], payload.song_length[s], track)) return 0;
        MusicSongPlayer player;
        player.start(track, bank, chip, /*loop=*/true);
        const size_t blocks = size_t(seconds * double(rate) / double(chunk));
        for (size_t b = 0; b < blocks; ++b) {
            player.render(buf.data(), chunk, rate, volume_to_gain_q15(80));
            for (int16_t v : buf) f.add16(uint16_t(v));
        }
    }
    return f.h;
}

/**
 * The chip alone, driven by a seeded stream of arbitrary register writes
 * interleaved with generate() calls of random lengths: every register the
 * emulator decodes (rates mid-envelope, KSR, AM/VIB, depth bits, every
 * waveform, F-number/block with and without key changes, feedback and
 * connection, KSL/TL, the OPL3 enable and its second array), so a cached
 * per-operator value that misses an invalidation shows up here even where
 * the patch's songs never go. Hashes the raw float bits of both outputs.
 */
inline uint64_t chip_stress(openu5::OplChipKind kind, uint32_t seed, int rounds) {
    using namespace openu5;
    OplEmulator chip(kind);
    uint32_t x = seed;
    auto next = [&x]() {
        x = x * 1664525u + 1013904223u;
        return x >> 8;
    };
    static const uint8_t kBases[] = {0x20, 0x40, 0x60, 0x80, 0xe0};
    Fnv f;
    std::vector<float> l(700), r(700);
    for (int round = 0; round < rounds; ++round) {
        const int writes = int(next() % 12);
        for (int w = 0; w < writes; ++w) {
            const uint32_t pick = next() % 100;
            const uint16_t array = (kind == OplChipKind::Opl3 && (next() & 1)) ? 0x100 : 0;
            uint16_t reg;
            if (pick < 40) reg = uint16_t(kBases[next() % 5] + next() % 0x16);          // operator registers
            else if (pick < 75) reg = uint16_t(0xa0 + (next() & 0x10) + next() % 9);    // F-number / key+block
            else if (pick < 90) reg = uint16_t(0xc0 + next() % 9);                      // feedback / connection
            else if (pick < 96) reg = 0xbd;                                              // tremolo / vibrato depth
            else reg = uint16_t(kind == OplChipKind::Opl3 ? 0x105 : 0x01);              // OPL3 enable / wave enable
            uint8_t value = uint8_t(next());
            // Key-on often: a chip with nothing keyed would hash silence.
            if ((reg & 0xf0) == 0xb0 && (next() % 3)) value |= 0x20;
            if (reg == 0x105) value = uint8_t(next() % 4 ? 1 : 0);
            chip.write_reg(uint16_t(array | reg), value);
        }
        const size_t n = 1 + next() % 690;
        const size_t off = next() % 8;
        chip.generate(l.data(), r.data(), n, off);
        for (size_t i = off; i < off + n; ++i) {
            f.addf(l[i]);
            f.addf(r[i]);
        }
    }
    return f.h;
}

// ---- the pinned values (A3-04A synth, before any A3-04B change) ----
constexpr uint64_t kCorpusL6 = 0x6a7ff3d5727df2c6ull;          // == A3-04A's documented L6 print
constexpr uint64_t kCorpusDevice128 = 0x52875b74637a7da9ull;                        // OPL2, 128 frames, 16 kHz, 30 s/song
constexpr uint64_t kCorpusOpl3 = 0x4116e61779750372ull;                             // OPL3, 128 frames, 16 kHz, 10 s/song
constexpr uint64_t kCorpusRates = 0x51b361b865686794ull;                            // OPL2: 44.1 kHz/100, 22.05 kHz/100, 16 kHz/96 frames, combined
constexpr uint64_t kChipStressOpl2 = 0xe35cff74c7f0587bull;
constexpr uint64_t kChipStressOpl3 = 0xdf50a8ed1111170aull;

} // namespace a3_04b
