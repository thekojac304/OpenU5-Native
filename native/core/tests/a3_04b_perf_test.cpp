// Alpha 3 A3-04B -- music CPU contention, the render side and the report
// (ALPHA3_AUDIO.md section 19). Host side:
//
//   G  the synth's A3-04B fast paths are bit-exact: six fingerprints computed
//      from the A3-04A synth before any change (tests/a3_04b_synth_goldens.h),
//      the silent floor, and the resampler's integer rounding against libm
//   Z  Music Volume 0 %: the synth does no work at all, the context is kept,
//      and raising the volume restarts the context's song from its start
//   C  the mix-saturation counter against an independent oracle
//   R  the render counters' arithmetic
//   F  the combined report: every field the device retest reads, every line
//      fits a Developer screen row
//   M  the two-consumer scheduling model: the producer renders only what the
//      DMA consumes and sleeps otherwise, muted music costs nothing, the
//      contention headroom of the A3-04A and A3-04B synth costs, and the
//      SFX / switch latency bounds under the new cost
//   I  the firmware's IRAM placement and flags (source scans)
//
//   a3_04b_perf <native/core dir> <native/assets/openu5-audio.bin> [--exhaustive]
//
// --exhaustive checks the integer rounding against std::lround for EVERY
// float in [-1, 1] (about two billion values, minutes); the ctest runs a
// dense sample.
#include "openu5/audio.h"
#include "openu5/audio_pack.h"
#include "openu5/audio_stream.h"
#include "openu5/music_synth.h"
#include "openu5/perf_report.h"
#include "openu5/sfx_synth.h"
#include "a3_04a_virtual_i2s_ring.h"
#include "a3_04b_synth_goldens.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace openu5;
using openu5_test::VirtualI2sRing;
using openu5_test::ring_clock;

namespace {
int checks = 0, failures = 0;
int32_t g_music_peak = 0; // the whole corpus at unity gain (G1 measures it)
void check(bool good, const std::string &label) {
    ++checks;
    if (!good) ++failures;
    std::printf("%s %s\n", good ? "GREEN" : "RED", label.c_str());
}
std::string slurp(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}
std::string strip_comments(const std::string &src) {
    std::string out;
    std::istringstream in(src);
    for (std::string line; std::getline(in, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        // "//" comments (C++), and whole lines starting with '#' (CMake, linker fragments).
        const auto first = line.find_first_not_of(" \t");
        const auto c = first != std::string::npos && line[first] == '#' ? first : line.find("//");
        out += (c == std::string::npos ? line : line.substr(0, c)) + "\n";
    }
    return out;
}
std::string hex(uint64_t v) {
    char b[24];
    std::snprintf(b, sizeof b, "%016llx", (unsigned long long)v);
    return b;
}

SfxRequest sfx_request(SfxId id, uint32_t sequence, uint16_t gain = kUnityGainQ15) {
    SfxRequest r{};
    r.id = id;
    r.gain_q15 = gain;
    r.sequence = sequence;
    return r;
}

// A sink that plays one block per write (always one short of full): every
// step renders exactly one block, so work can be counted block by block.
class FreeSink final : public PcmRingSink {
  public:
    bool preload(const int16_t *) override { return ++preloaded <= kAudioRingBlocks; }
    bool enable() override {
        preloaded = 0;
        return true;
    }
    void disable() override {}
    bool write(const int16_t *block) override {
        ++played_;
        last.assign(block, block + kAudioBlockFrames);
        return true;
    }
    uint32_t blocks_played() const override { return played_; }
    uint32_t underrun_events() const override { return 0; }
    uint32_t preloaded = 0;
    std::vector<int16_t> last;

  private:
    uint32_t played_ = 0;
};

// The device backend's shape (tdeck_audio.cpp), without FreeRTOS: the
// service's calls land on the production pump the way the device's queues
// deliver them between blocks.
class PumpBackend final : public AudioBackend {
  public:
    explicit PumpBackend(const MusicLibrary *library) { pump.set_music_library(library); }
    bool play_sfx(const SfxRequest &r) override {
        if (!sfx_supported(r.id)) return false;
        ++sfx;
        pump.sync_epoch(epoch);
        pump.submit_sfx(r, epoch);
        return true;
    }
    void stop_sfx() override {
        ++epoch;
        pump.sync_epoch(epoch);
    }
    bool start_music(MusicSong s, uint16_t gain) override {
        ++starts;
        last = s;
        music_gain = gain;
        pump.play_music(s);
        return true;
    }
    void stop_music() override {
        ++stops;
        pump.stop_music();
    }
    void set_gain(AudioChannel c, uint16_t gain) override { (c == AudioChannel::Sfx ? sfx_gain : music_gain) = gain; }

    /** One block through the pump; true if the music synth ran for it. */
    bool step(PcmRingSink &sink) {
        const bool music = pump.music().active();
        pump.step(sink, sfx_gain, music_gain);
        return music;
    }

    AudioRingPump pump{};
    uint16_t sfx_gain = 0, music_gain = 0;
    uint32_t epoch = 0;
    int starts = 0, stops = 0, sfx = 0;
    MusicSong last = MusicSong::None;
};

std::vector<std::vector<int16_t>> fresh_blocks(const MusicLibrary &lib, MusicSong song, uint16_t gain, size_t n) {
    MusicSongPlayer p;
    p.start(*lib.track(song), lib.bank(), kDeviceMusicChip, /*loop=*/true);
    std::vector<std::vector<int16_t>> out;
    std::vector<int16_t> b(kAudioBlockFrames);
    for (size_t i = 0; i < n; ++i) {
        p.render(b.data(), kAudioBlockFrames, kSfxOutputRateHz, gain);
        out.push_back(b);
    }
    return out;
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: a3_04b_perf <native/core dir> <openu5-audio.bin> [--exhaustive]\n");
        return 2;
    }
    const std::string core_dir = argv[1];
    const std::string device_dir = core_dir + "/../targets/tdeck";
    const bool exhaustive = argc > 3 && std::strcmp(argv[3], "--exhaustive") == 0;

    const std::string pack_bytes = slurp(argv[2]);
    std::vector<uint8_t> pack(pack_bytes.begin(), pack_bytes.end());
    AudioPackPayload payload{};
    const AudioPackInfo info = inspect_audio_pack(pack.data(), pack.size(), &payload);
    check(!pack.empty() && info.state == AudioPackState::Valid &&
              info.record.capability == MusicCapability::SupportedMusicPatch,
          "G0 the real audio pack is present, Valid and Supported (run: npm run pack:audio)");
    MilesOplBank bank;
    bank.load(payload.bank, payload.bank_length);
    MusicLibrary library;
    library.load(&payload);

    // ======================================================================
    // G -- the fast paths are bit-exact (section 19.7)
    // ======================================================================
    {
        const uint64_t l6 = a3_04b::corpus_l6(payload, bank, &g_music_peak);
        check(l6 == a3_04b::kCorpusL6, "G1 A3-04's own L6 corpus fingerprint is unchanged: " + hex(l6) +
                                           " (A3-04A documented 6a7ff3d5727df2c6)");
        const uint64_t dev = a3_04b::corpus_stream(payload, bank, OplChipKind::Opl2, 128, 16000, 30.0);
        check(dev == a3_04b::kCorpusDevice128,
              "G2 the device's geometry -- every song looping 30 s in 128-frame blocks at 16 kHz, Music 80 %: " + hex(dev));
        const uint64_t opl3 = a3_04b::corpus_stream(payload, bank, OplChipKind::Opl3, 128, 16000, 10.0);
        check(opl3 == a3_04b::kCorpusOpl3, "G3 the same corpus through the OPL3 chip (stereo bus): " + hex(opl3));
        const uint64_t rates = a3_04b::corpus_stream(payload, bank, OplChipKind::Opl2, 100, 44100, 4.0) ^
                               (a3_04b::corpus_stream(payload, bank, OplChipKind::Opl2, 100, 22050, 4.0) * 3) ^
                               (a3_04b::corpus_stream(payload, bank, OplChipKind::Opl2, 96, 16000, 4.0) * 7);
        check(rates == a3_04b::kCorpusRates, "G4 other output rates and chunk sizes (44.1 kHz, 22.05 kHz, 96 frames): " +
                                                 hex(rates));
        const uint64_t s2 = a3_04b::chip_stress(OplChipKind::Opl2, 0x5eed04b, 6000);
        check(s2 == a3_04b::kChipStressOpl2,
              "G5 the OPL2 chip under 6,000 rounds of random register writes (every cached input changed mid-note): " + hex(s2));
        const uint64_t s3 = a3_04b::chip_stress(OplChipKind::Opl3, 0xa3a3b0b, 6000);
        check(s3 == a3_04b::kChipStressOpl3,
              "G6 the OPL3 chip under the same, both arrays and all eight waveforms: " + hex(s3));

        // The silent floor is a property of the exp ROM, not a guess.
        int rom_max = 0;
        for (int i = 0; i < 256; ++i) rom_max = std::max(rom_max, int(test_only::opl_exp(i)));
        bool zero_above = true;
        for (int32_t att = OplEmulator::kSilentFloor; att <= 0x2400; ++att) zero_above = zero_above && test_only::opl_expo(att) == 0;
        check(rom_max + 2048 < 4096 && zero_above && test_only::opl_expo(OplEmulator::kSilentFloor - 1) > 0,
              "G7 kSilentFloor = 3072 is exact: exp ROM max " + std::to_string(rom_max) +
                  " + 2048 < 4096, so expo() is 0 from 3072 up and non-zero just below");

        // The resampler's integer rounding is lround, value for value.
        uint64_t tested = 0, wrong = 0;
        auto test_round = [&](float v) {
            ++tested;
            const long want = std::lround(double(v) * 32767.0);
            if (long(test_only::opl_round_q15(v)) != want) ++wrong;
        };
        if (exhaustive) {
            for (uint64_t bits = 0; bits <= 0x3f800000u; ++bits) { // 0 .. 1.0, then the negatives
                float v;
                const uint32_t b32 = uint32_t(bits);
                std::memcpy(&v, &b32, sizeof v);
                test_round(v);
                test_round(-v);
            }
        } else {
            uint32_t x = 12345;
            for (int i = 0; i < 16000000; ++i) {
                x = x * 1664525u + 1013904223u;
                float v;
                const uint32_t b32 = (x & 0x80000000u) | (x % 0x3f800001u);
                std::memcpy(&v, &b32, sizeof v);
                test_round(v);
            }
            // Every half-way point k + 1/2 the float grid can reach, and its neighbours.
            for (int k = -32768; k <= 32767; ++k) {
                const float v = float((double(k) + 0.5) / 32767.0);
                test_round(v);
                test_round(std::nextafter(v, 2.0f));
                test_round(std::nextafter(v, -2.0f));
            }
            for (float v : {0.0f, -0.0f, 1.0f, -1.0f, 1e-30f, -1e-30f, 1.401298e-45f, 0.5f / 32767.0f})
                test_round(v);
        }
        check(wrong == 0 && tested > 1000000,
              "G8 round_q15 == std::lround(double(v) * 32767.0) for " + std::to_string(tested) + " floats" +
                  (exhaustive ? " (EVERY float in [-1, 1])" : " (random, every half-way point, edges)") + ", " +
                  std::to_string(wrong) + " differ");
        uint64_t cwrong = 0;
        uint32_t y = 777;
        for (int i = 0; i < 2000000; ++i) {
            y = y * 1664525u + 1013904223u;
            const double v = (i % 3 == 0) ? double(y % 100000) : double(y) / 977.0 + 1e-9;
            if (v <= 0) continue;
            if (test_only::opl_ceil_positive(v) != size_t(std::ceil(v))) ++cwrong;
        }
        check(cwrong == 0, "G9 ceil_positive == std::ceil for 2,000,000 positive doubles, whole numbers included");
    }

    // ======================================================================
    // Z -- Music Volume 0 % (section 19.6)
    // ======================================================================
    {
        PumpBackend backend(&library);
        AudioService service;
        service.set_music_availability(MusicAvailability::Available);
        service.attach(&backend);
        service.set_music_volume(80);
        service.play_music(MusicContext::Overworld);
        FreeSink sink;
        int music_blocks = 0;
        for (int i = 0; i < 100; ++i) music_blocks += backend.step(sink);
        check(backend.starts == 1 && backend.last == MusicSong::BritannicLands && music_blocks == 100,
              "Z0 Music 80 %: the overworld song plays, the synth runs for every block");

        service.set_music_volume(0);
        int muted_music = 0, rendered_after = 0;
        for (int i = 0; i < 200; ++i) {
            const bool sleeping = backend.pump.sleeping();
            if (!sleeping) ++rendered_after;
            muted_music += backend.step(sink);
        }
        check(backend.stops == 1 && muted_music == 0 && service.current_song() == MusicSong::None &&
                  service.current_music_context() == MusicContext::Overworld,
              "Z1 Music 0 %: stop_music reaches the backend, the synth runs for NO block, and the context is kept");
        check(backend.pump.sleeping() && rendered_after <= int(kAudioRingBlocks) + 1,
              "Z1 ... and with no cue the pump drains one ring of silence and sleeps: " + std::to_string(rendered_after) +
                  " blocks after the mute, then nothing (A3-04A's '0 % is much faster')");

        // Unmute: the context's song restarts from its first block.
        service.set_music_volume(80);
        const auto fresh = fresh_blocks(library, MusicSong::BritannicLands, volume_to_gain_q15(80), 40);
        bool same = backend.starts == 2 && backend.last == MusicSong::BritannicLands;
        for (size_t i = 0; same && i < fresh.size(); ++i) {
            backend.step(sink);
            // The pump primes the ring first (preload, not write): compare what it rendered.
            const int16_t *b = backend.pump.last_block();
            same = std::equal(b, b + kAudioBlockFrames, fresh[i].begin());
        }
        check(same, "Z2 Music back to 80 %: the context's song restarts at its first block (restart policy: 40 blocks "
                    "equal a fresh start, bit for bit)");

        // A context change while muted reaches nothing; unmuting plays the NEW context.
        service.set_music_volume(0);
        const int starts_before = backend.starts;
        service.play_music(MusicContext::Combat);
        service.play_music(MusicContext::Dungeon);
        check(backend.starts == starts_before && service.current_music_context() == MusicContext::Dungeon,
              "Z3 contexts change while muted without touching the backend (no start, no synth)");
        service.flush_for_load();
        const uint32_t epoch_after_load = backend.epoch;
        check(epoch_after_load >= 1 && backend.starts == starts_before,
              "Z4 a load while muted flushes SFX (epoch " + std::to_string(epoch_after_load) + ") and starts no music");
        // Let the muted pump drain and go to sleep first: a cue must wake it.
        int drained = 0;
        while (!backend.pump.sleeping() && drained < 100) {
            backend.step(sink);
            ++drained;
        }
        const bool slept = backend.pump.sleeping();
        service.play_sfx(SfxId::MoveStep);
        bool heard = false;
        int music_while_muted = 0;
        for (int i = 0; i < 30; ++i) {
            music_while_muted += backend.step(sink);
            const int16_t *b = backend.pump.last_block();
            for (size_t k = 0; k < kAudioBlockFrames; ++k) heard = heard || b[k] != 0;
        }
        check(slept && backend.sfx == 1 && heard && music_while_muted == 0,
              "Z5 SFX are unaffected while muted: a step played to the SLEEPING pump wakes it and is heard; the synth "
              "still runs for no block");
        service.set_music_volume(80);
        check(backend.starts == starts_before + 1 && backend.last == MusicSong::HallsOfDoom,
              "Z6 unmuting after the context changes plays the current context's song (Halls of Doom), once");
    }

    // ======================================================================
    // C -- the mix-saturation counter (section 19.10)
    // ======================================================================
    {
        // The pump against an independent sum, block for block: `song` with
        // `cue` every 13 blocks, each at its own gain.
        auto clip_run = [&](MusicSong song, SfxId cue, uint16_t music_gain, uint16_t sfx_gain, uint32_t &counted,
                            uint32_t &oracle, uint32_t &peak) {
            AudioRingPump pump;
            pump.set_music_library(&library);
            FreeSink sink;
            pump.play_music(song);
            MusicSongPlayer music;
            music.start(*library.track(song), library.bank(), kDeviceMusicChip, true);
            SfxPlayer sfx;
            int16_t mb[kAudioBlockFrames], sb[kAudioBlockFrames];
            oracle = 0;
            peak = 0;
            for (uint32_t k = 0; k < 2500; ++k) {
                if (k % 13 == 3) {
                    pump.submit_sfx(sfx_request(cue, k, sfx_gain), 0);
                    sfx.submit(sfx_request(cue, k, sfx_gain), 0);
                }
                pump.step(sink, sfx_gain, music_gain);
                music.render(mb, kAudioBlockFrames, kSfxOutputRateHz, music_gain);
                sfx.render(sb, kAudioBlockFrames, sfx_gain);
                for (size_t i = 0; i < kAudioBlockFrames; ++i) {
                    const int32_t sum = int32_t(mb[i]) + int32_t(sb[i]);
                    oracle += (sum > 32767 || sum < -32768) ? 1u : 0u;
                    peak = std::max(peak, uint32_t(std::abs(sum)));
                }
            }
            AudioPerfSnapshot s{};
            pump.perf(s);
            counted = s.mix_clipped;
        };
        // The loudest song (Lord Blackthorn) and the loudest cue (the shop's), both at 100 %.
        uint32_t counted = 0, oracle = 0, peak = 0;
        clip_run(MusicSong::Blackthorn, SfxId::ShopTransaction, kUnityGainQ15, kUnityGainQ15, counted, oracle, peak);
        check(oracle > 0 && counted == oracle,
              "C1 Music 100 % + SFX 100 %, the loudest song under the loudest cue: the pump counts exactly the "
              "saturated samples an independent sum finds (" + std::to_string(counted) + ", peak " + std::to_string(peak) + ")");
        uint32_t fc = 0, foracle = 0, fpeak = 0;
        clip_run(MusicSong::Engagement, SfxId::CombatHitHeavy, kUnityGainQ15, kUnityGainQ15, fc, foracle, fpeak);
        check(foracle == 0 && fc == 0,
              "C2 a fight -- the combat song under a heavy hit every 104 ms -- never saturates, even at 100 % / 100 % "
              "(peak " + std::to_string(fpeak) + ")");
        // The bound behind "combat static is not clipping": every song's and
        // every cue's own peak, at the default volumes.
        int32_t sfx_peak = 0;
        SfxId loudest = SfxId::None;
        for (size_t id = 1; id < kSfxIdCount; ++id) {
            if (!sfx_supported(SfxId(id))) continue;
            for (int32_t param : {0, 4, 8}) {
                SfxPlayer sp;
                SfxRequest r = sfx_request(SfxId(id), 1);
                r.param = param;
                sp.submit(r, 0);
                int16_t b[kAudioBlockFrames];
                for (int k = 0; k < 125 * 20 && !sp.idle(); ++k) {
                    sp.render(b, kAudioBlockFrames, kUnityGainQ15);
                    for (int16_t v : b)
                        if (std::abs(int(v)) > sfx_peak) {
                            sfx_peak = std::abs(int(v));
                            loudest = SfxId(id);
                        }
                }
            }
        }
        const int32_t at_defaults = apply_gain_q15(int16_t(g_music_peak), volume_to_gain_q15(kDefaultMusicVolume)) +
                                    apply_gain_q15(int16_t(sfx_peak), volume_to_gain_q15(kDefaultSfxVolume));
        std::printf("  corpus peak %d (music, unity), loudest cue %s %d (unity); at the default volumes at most %d\n",
                    int(g_music_peak), sfx_cue(loudest), int(sfx_peak), int(at_defaults));
        check(g_music_peak > 20000 && sfx_peak > 8000 && at_defaults < 32768,
              "C3 at the default volumes NO song under NO cue can saturate: loudest song " + std::to_string(g_music_peak) +
                  " + loudest cue " + std::to_string(sfx_peak) + " at 80 % / 80 % = " + std::to_string(at_defaults) +
                  " < 32768 -- combat static is not clipping (the counter now says so on the device)");
    }

    // ======================================================================
    // R -- the render counters (section 19.5)
    // ======================================================================
    {
        RenderPerfCounters c;
        c.reset(1000000);
        // 100 frames, 10 ms apart, taking 1..100 ms: frames above 55 ms are late.
        for (uint32_t i = 1; i <= 100; ++i)
            c.on_frame(1000000 + uint64_t(i) * 10000, i * 400, i * 100, i * 600, i * 1000);
        c.on_input(3000);
        c.on_input(9000);
        c.on_input_shown(40000);
        c.on_input_shown(120000);
        RenderPerfSnapshot s{};
        c.snapshot(1000000 + 2000000, s);
        check(s.frames == 100 && s.frame_avg_us == 50500 && s.frame_max_us == 100000 && s.late_frames == 45 &&
                  s.compose_max_us == 40000 && s.tiles_max_us == 10000 && s.tft_avg_us == 30300 && s.tft_max_us == 60000,
              "R1 frames, their average / maximum, compose / tiles / TFT, and late frames (> 55 ms: 45 of them)");
        check(s.frame_p95_us == 96000 && s.frame_p99_us == 100000,
              "R2 p95 / p99 = the upper edge of the 2 ms bucket holding rank 95 / 99, never above the maximum");
        check(s.cadence_avg_us == 10000 && s.cadence_max_us == 10000 && s.inputs == 2 && s.handle_max_us == 9000 &&
                  s.shown == 2 && s.input_avg_us == 80000 && s.input_max_us == 120000,
              "R3 the frame cadence, input handling and input -> screen latency");
        check(s.window_us == 2000000 && s.busy_permille == uint32_t((5050000ull + 12000) * 1000 / 2000000),
              "R4 game busy = (frames + input handling) / window");
        RenderPerfCounters big;
        big.reset(0);
        big.on_frame(0, 0, 0, 0, 500000);
        RenderPerfSnapshot b{};
        big.snapshot(1000000, b);
        c.reset(5);
        RenderPerfSnapshot z{};
        c.snapshot(5, z);
        check(b.frame_p99_us == 500000 && b.late_frames == 1 && z.frames == 0 && z.frame_max_us == 0 && z.shown == 0,
              "R5 a frame past the histogram is its own percentile; a reset starts an empty window");
    }

    // ======================================================================
    // F -- the combined report (section 19.3)
    // ======================================================================
    {
        AudioPerfSnapshot a{};
        a.music_active = true;
        a.song = MusicSong::Theme;
        a.window_us = 30000000;
        a.blocks = 3750;
        a.render_avg_us = 3210;
        a.render_p99_us = 4100;
        a.render_max_us = 5020;
        a.music_avg_us = 3000;
        a.music_max_us = 4800;
        a.cpu_permille = 401;
        a.missed_deadlines = 1;
        a.underruns = 2;
        a.hw_underruns = 3;
        a.mix_clipped = 4;
        a.fill_min = 7;
        a.fill_max = 8;
        a.period_max_us = 9100;
        a.voices_max = 9;
        a.channels_avg_x100 = 861;
        a.channels_max = 9;
        a.stack_free_min = 3120;
        RenderPerfSnapshot r{};
        r.window_us = 45000000;
        r.frames = 250;
        r.frame_avg_us = 42100;
        r.frame_p95_us = 70000;
        r.frame_p99_us = 80000;
        r.frame_max_us = 120400;
        r.late_frames = 3;
        r.tft_avg_us = 30200;
        r.tft_max_us = 90100;
        r.input_max_us = 130000;
        r.shown = 12;
        r.busy_permille = 450;
        SystemPerfSnapshot sys{};
        sys.valid = true;
        sys.core_busy_permille[0] = 780;
        sys.core_busy_permille[1] = 550;
        sys.audio_permille = 510;
        sys.main_permille = 700;
        sys.heap_internal_free = 123456;
        sys.heap_psram_free = 4567890;
        sys.stack_main_free = 9870;
        PerfReportInput in{};
        in.title = "AUDIO/RENDER PERF  benchmark";
        in.audio = &a;
        in.audio_heading = "Music alone (idle)";
        in.audio2 = &a;
        in.audio2_heading = "Music + cue/100 ms";
        in.render = &r;
        in.system = &sys;
        in.has_guard = true;
        in.guard_ns = 900;
        in.guard_us_per_block = 12500;
        char lines[kPerfReportMaxLines][kPerfReportLineBytes];
        const size_t n = format_perf_report(in, lines, kPerfReportMaxLines);
        std::string all;
        for (size_t i = 0; i < n; ++i) all += std::string(lines[i]) + "\n";
        const char *needles[] = {"render avg 3.21 p99 4.10 max 5.02 ms", "missed 1  underrun 2  hw 3  clip 4",
                                 "buffered min 56 max 64 ms", "sched max 9.1", "voices max 9", "OPL ch avg 8.6 max 9",
                                 "audio CPU 40%", "audio stack min 3120 B", "frame avg 42.1 p95 70.0 p99 80.0",
                                 "frame max 120.4 ms  late (>55 ms) 3", "tft avg 30.2 max 90.1 ms", "max 130.0 ms",
                                 "game busy 45%", "CPU0 78%  CPU1 55%", "audio 51% main 70%", "heap int 123456",
                                 "heap PSRAM 4567890", "stack free B: main 9870", "OPL2 49716 Hz -> 16000 Hz out, ring 8x128",
                                 "Music alone (idle): Ultima V Theme", "Music + cue/100 ms", "A3-04 guard 900 ns/read"};
        std::string missing;
        for (const char *needle : needles)
            if (all.find(needle) == std::string::npos) missing += std::string(" [") + needle + "]";
        check(missing.empty(), "F1 the report carries every field the device retest reads (render, sched, missed, "
                               "underrun/hw, buffered min/max, voices, clip, CPU, stacks, heap, PSRAM, rates, frames, "
                               "TFT, late, input)" + missing);
        std::printf("%s", all.c_str());

        // Worst case: every field at its maximum -- still one row each.
        AudioPerfSnapshot big{};
        std::memset(static_cast<void *>(&big), 0xff, sizeof big);
        big.music_active = true;
        big.song = MusicSong::Hornpipe;
        RenderPerfSnapshot rbig{};
        std::memset(static_cast<void *>(&rbig), 0xff, sizeof rbig);
        SystemPerfSnapshot sbig{};
        std::memset(static_cast<void *>(&sbig), 0xff, sizeof sbig);
        sbig.valid = true;
        PerfReportInput worst = in;
        worst.audio = worst.audio2 = &big;
        worst.render = &rbig;
        worst.system = &sbig;
        worst.guard_ns = worst.guard_us_per_block = UINT32_MAX;
        char wl[kPerfReportMaxLines][kPerfReportLineBytes];
        const size_t wn = format_perf_report(worst, wl, kPerfReportMaxLines);
        bool fits = wn > 0 && wn <= kPerfReportMaxLines;
        for (size_t i = 0; i < wn; ++i) fits = fits && std::strlen(wl[i]) < kPerfReportLineBytes;
        char two[2][kPerfReportLineBytes];
        std::memset(two, 0x5a, sizeof two);
        const size_t tn = format_perf_report(in, two, 1);
        check(fits && tn == 1 && two[1][0] == 0x5a,
              "F2 every field at its maximum still fits one 51-character row per line (" + std::to_string(wn) +
                  " lines), and max_lines is never exceeded");
        PerfReportInput bare{};
        bare.title = "AUDIO/RENDER PERF  live window";
        SystemPerfSnapshot none{};
        bare.system = &none;
        RenderPerfSnapshot idle{};
        bare.render = &idle;
        char bl[kPerfReportMaxLines][kPerfReportLineBytes];
        const size_t bn = format_perf_report(bare, bl, kPerfReportMaxLines);
        std::string ball;
        for (size_t i = 0; i < bn; ++i) ball += std::string(bl[i]) + "\n";
        check(ball.find("no audio played") != std::string::npos && ball.find("no run-time statistics") != std::string::npos &&
                  ball.find("none drawn") != std::string::npos,
              "F3 no audio, no frames and no run-time statistics are each said, not left blank");
    }

    // ======================================================================
    // M -- the two-consumer scheduling model (section 19.11)
    // ======================================================================
    // Core 1 runs the audio task: the production pump against the ESP-IDF
    // ring model, with a per-block cost from a cost model (what the synth
    // does per sounding channel per chip sample, from the linked images'
    // listings, section 19.7). Core 0 runs the game loop. The only couplings
    // the architecture allows are the shared caches (modelled as an inflation
    // of the synth's cost while it runs from flash) and the DMA interrupt.
    {
        struct CostModel {
            double ns_per_channel_sample; // the synth, per sounding channel per chip sample
            double fixed_us;              // per block: resampler, SFX, mix, bookkeeping
        };
        // Static estimates from the ESP32-S3 listings at 240 MHz (section 19.7),
        // both at 1.2 cycles per instruction: A3-04A's per-sounding-channel
        // path ~254 instructions (four out-of-line calls, 64-bit phase steps)
        // = ~305 cycles, before any flash-cache miss; A3-04B's ~155, from
        // IRAM = ~186. The retest's "music avg/max" measures the real ones.
        const CostModel a3_04a{305.0 / 0.240, 350.0}, a3_04b{186.0 / 0.240, 250.0};
        auto block_cost = [&](const CostModel &m, const AudioRingPump &p, double inflation) {
            const double chip_samples = double(kAudioBlockFrames) * kOplClockHz / kSfxOutputRateHz;
            const double synth = p.music().active() ? m.ns_per_channel_sample * double(p.music().sounding_channels()) *
                                                          chip_samples / 1000.0
                                                    : 0.0;
            return uint64_t((synth * inflation + m.fixed_us));
        };
        struct Result {
            uint64_t busy_us = 0, wall_us = 0;
            uint32_t rendered = 0, music_rendered = 0, dry = 0;
            AudioPerfSnapshot perf{};
        };
        auto simulate = [&](const CostModel &m, double inflation, uint32_t steps, bool music, uint32_t seed) {
            Result r;
            VirtualI2sRing ring(kAudioRingBlocks);
            AudioRingPump pump;
            pump.set_music_library(&library);
            pump.set_clock({&ring, &ring_clock});
            pump.reset_perf();
            if (music) pump.play_music(MusicSong::Theme);
            uint32_t x = seed;
            for (uint32_t k = 0; k < steps; ++k) {
                if (k % 25 == 7) pump.submit_sfx(sfx_request(SfxId::CombatHit, k), 0);
                if (pump.sleeping()) {
                    ring.advance(20000);
                    continue;
                }
                // Inflation arrives in bursts (a combat frame's worth of cache
                // pressure) on a random ~30 % of blocks.
                x = x * 1664525u + 1013904223u;
                const double burst = (x >> 8) % 10 < 3 ? inflation : 1.0;
                const uint64_t cost = block_cost(m, pump, burst);
                if (pump.music().active()) ++r.music_rendered;
                r.busy_us += cost;
                ++r.rendered;
                ring.advance(cost);
                if (pump.step(ring, kUnityGainQ15, volume_to_gain_q15(80))) ring.advance(10000);
            }
            r.wall_us = ring.now();
            r.dry = ring.dry_plays;
            pump.perf(r.perf);
            return r;
        };
        const Result on = simulate(a3_04b, 1.0, 2500, true, 1);
        // on.perf.blocks counts the pump's own renders (every render_block
        // call); written counts the blocks handed to the DMA after priming.
        check(on.dry == 0 && on.perf.blocks == on.perf.written + kAudioRingBlocks && on.perf.blocks == on.rendered &&
                  on.rendered + 2 >= uint32_t(on.wall_us / kAudioBlockUs),
              "M1 the producer renders exactly what the DMA consumes (" + std::to_string(on.perf.blocks) +
                  " renders = one primed ring + " + std::to_string(on.perf.written) + " writes, in " +
                  std::to_string(on.wall_us / 1000) + " ms): it never runs ahead of the ring, it waits in the write");
        const double duty = double(on.busy_us) / double(on.wall_us);
        check(duty < 0.6 && on.perf.underruns == 0 && on.perf.hw_underruns == 0,
              "M1 ... so the audio task's CPU is the synth's own cost and nothing more: " +
                  std::to_string(int(duty * 100)) + " % of core 1 for the Theme at the A3-04B cost");
        const Result muted = simulate(a3_04b, 1.0, 2500, false, 1);
        check(muted.music_rendered == 0 && double(muted.busy_us) / double(muted.wall_us) < 0.05,
              "M2 music muted: the synth runs for no block and core 1 is ~idle between cues (" +
                  std::to_string(int(double(muted.busy_us) * 1000 / double(muted.wall_us))) + " permille)");

        // How much cache inflation (in bursts) each synth cost survives with no dry block.
        auto headroom = [&](const CostModel &m) {
            double ok = 1.0;
            for (double f = 1.0; f <= 8.0; f += 0.05) {
                if (simulate(m, f, 1500, true, 7).dry != 0) break;
                ok = f;
            }
            return ok;
        };
        const double h_a = headroom(a3_04a), h_b = headroom(a3_04b);
        const Result old_cost = simulate(a3_04a, 1.0, 2500, true, 1);
        std::printf("  model: Theme at the A3-04A cost %d %% of core 1, at the A3-04B cost %d %%; burst inflation "
                    "tolerated before an underrun: A3-04A %.2fx, A3-04B %.2fx\n",
                    int(double(old_cost.busy_us) * 100 / double(old_cost.wall_us)), int(duty * 100), h_a, h_b);
        // The expected ratio is the synth-cost ratio (305 / 186 = 1.64), less
        // what the unchanged per-block costs take: at least 1.5.
        check(h_b >= 1.5 * h_a,
              "M3 the A3-04B synth survives far more contention than A3-04A's before the speaker runs dry (" +
                  std::to_string(h_a).substr(0, 4) + "x -> " + std::to_string(h_b).substr(0, 4) + "x) -- and in IRAM it "
                  "takes none from the flash cache in the first place");

        // SFX and switch latency bounds hold at the new cost.
        {
            VirtualI2sRing ring(kAudioRingBlocks);
            AudioRingPump pump;
            pump.set_music_library(&library);
            pump.set_clock({&ring, &ring_clock});
            pump.play_music(MusicSong::Theme);
            uint64_t cue_at = 0, heard_at = 0;
            size_t cue_block = 0;
            for (uint32_t k = 0; k < 400; ++k) {
                if (k == 200) {
                    cue_at = ring.now();
                    pump.submit_sfx(sfx_request(SfxId::DiagnosticTone, k), 0);
                    cue_block = ring.written.size() + 1;
                }
                ring.advance(block_cost(a3_04b, pump, 1.0));
                pump.step(ring, kUnityGainQ15, 0); // music at 0 gain: only the cue is audible
            }
            for (const auto &p : ring.played)
                if (p.id >= cue_block && !p.silent && !heard_at) heard_at = p.start_us;
            const uint64_t latency = heard_at > cue_at ? heard_at - cue_at : UINT64_MAX;
            check(heard_at && latency <= uint64_t(kAudioRingBlocks + 1) * kAudioBlockUs,
                  "M4 a cue submitted while music plays at the A3-04B cost is heard within the ring + one block (" +
                      std::to_string(latency / 1000) + " ms <= 72 ms)");
        }
    }

    // ======================================================================
    // I -- the firmware's placement and flags (section 19.8)
    // ======================================================================
    {
        const std::string lf = strip_comments(slurp(device_dir + "/main/audio_iram.lf"));
        const char *hot[] = {"_ZN6openu511OplEmulator8generateEPfS1_jj", "_ZN6openu515MusicSongPlayer6renderEPsjmt",
                             "_ZN6openu515MusicSongPlayer9fill_chipEPfS1_jj", "_ZN6openu513AudioRingPump12render_blockEtt",
                             "_ZN6openu59SfxPlayer6renderEPsjt", "_ZN6openu512SpeakerVoice6renderEPlj",
                             "_ZN6openu514apply_gain_q15Est"};
        std::string missing;
        for (const char *h : hot)
            if (lf.find(std::string(h) + " (noflash)") == std::string::npos) missing += std::string(" ") + h;
        check(!lf.empty() && lf.find("archive: libmain.a") != std::string::npos && missing.empty(),
              "I1 the linker fragment puts the per-sample audio path in IRAM (noflash)" + missing);
        // A4-PARITY1 (NEW-3): SpeakerVoice::begin_segment (IRAM) calls this leaf on the audio
        // task. Once GCC stopped inlining it (by A4-END1), the flash copy put the per-sample
        // path back on the shared instruction cache and a3_04b_iram_check went RED.
        check(lf.find("sfx_synth:_ZN6openu522speaker_segment_framesERKNS_14SpeakerSegmentE (noflash)") !=
                  std::string::npos,
              "I1b the fragment also places speaker_segment_frames, begin_segment's leaf, in IRAM");
        const std::string cmake = strip_comments(slurp(device_dir + "/main/CMakeLists.txt"));
        check(std::regex_search(cmake, std::regex(R"(LDFRAGMENTS\s+"audio_iram\.lf")")) &&
                  std::regex_search(cmake, std::regex(R"(music_synth\.cpp[^)]*COMPILE_OPTIONS[^)]*-O2[^)]*-ffp-contract=off[^)]*-fno-jump-tables)")) &&
                  std::regex_search(cmake, std::regex(R"(sfx_synth\.cpp[^)]*COMPILE_OPTIONS[^)]*-fno-jump-tables)")),
              "I2 the component registers the fragment; the IRAM files are built without jump tables (a switch table "
              "would live in flash .rodata)");
        const std::string defaults = slurp(device_dir + "/sdkconfig.defaults");
        check(defaults.find("CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y") != std::string::npos &&
                  defaults.find("CONFIG_FREERTOS_RUN_TIME_COUNTER_TYPE_U64=y") != std::string::npos,
              "I3 the firmware keeps FreeRTOS run-time statistics (64-bit, esp_timer) for the per-core / per-task CPU");
        const std::string synth = strip_comments(slurp(core_dir + "/src/music_synth.cpp"));
        const auto g0 = synth.find("void OplEmulator::generate(");
        const auto g1 = synth.find("\n}\n", g0);
        const std::string gen = g0 == std::string::npos ? "" : synth.substr(g0, g1 - g0);
        check(!gen.empty() && gen.find("phase_inc(") == std::string::npos && gen.find("int64_t") == std::string::npos &&
                  gen.find("std::") == std::string::npos,
              "I4 generate() computes no phase step and no 64-bit value per sample, and calls no library function");
    }

    std::printf("A3-04B perf: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
