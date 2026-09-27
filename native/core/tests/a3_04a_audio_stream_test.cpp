// Alpha 3 A3-04A -- the real-time audio path, host side (ALPHA3_AUDIO.md
// section 18). The device's audio task now runs openu5::AudioRingPump; this
// test drives that same production code against a model of the ESP-IDF 6.1
// i2s_std TX descriptor ring and proves the architecture, not a desktop's
// speed:
//
//   H  the hot path: no function-local static (A3-04's per-sample mutex),
//      the firmware's per-file -O2, no blocking/allocating primitive
//   L  the music library: every song parsed once, at load
//   P  the performance counters' arithmetic
//   R  the pump against the driver model: prime-then-enable, no stale replay,
//      drain before off, exact playback order, stall tolerance, throughput
//      failure detected, bounded switch/volume latency, SFX overlay,
//      zero allocation per block and per switch
//   B  the Developer benchmark's timeline and its report lines
//   S  the device backend's wiring (source scans of tdeck_audio.cpp)
//
//   a3_04a_audio_stream <native/core dir> <native/assets/openu5-audio.bin>
#include "openu5/audio.h"
#include "openu5/audio_pack.h"
#include "openu5/audio_stream.h"
#include "openu5/music_synth.h"
#include "openu5/sfx_synth.h"
#include "a3_04a_virtual_i2s_ring.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <functional>
#include <new>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace openu5;

namespace {
int checks = 0, failures = 0;
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
std::vector<uint8_t> slurp_bytes(const std::string &path) {
    const std::string s = slurp(path);
    return std::vector<uint8_t>(s.begin(), s.end());
}
std::string strip_comments(const std::string &src) {
    std::string out;
    std::istringstream in(src);
    for (std::string line; std::getline(in, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto c = line.find("//");
        out += (c == std::string::npos ? line : line.substr(0, c)) + "\n";
    }
    return out;
}

long g_alloc_count = 0;
} // namespace

void *operator new(std::size_t n) {
    ++g_alloc_count;
    void *p = std::malloc(n ? n : 1);
    if (!p) throw std::bad_alloc();
    return p;
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
void *operator new[](std::size_t n) {
    ++g_alloc_count;
    void *p = std::malloc(n ? n : 1);
    if (!p) throw std::bad_alloc();
    return p;
}
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete[](void *p, std::size_t) noexcept { std::free(p); }

namespace {

// The driver model: tests/a3_04a_virtual_i2s_ring.h (shared with a3_04b_perf since A3-04B).
using openu5_test::VirtualI2sRing;
using openu5_test::ring_clock;


// A sink that allocates nothing (for the allocation proof): always accepts,
// and "plays" one block per write so the ring always looks one short of full.
class CountingSink final : public PcmRingSink {
  public:
    bool preload(const int16_t *) override { return ++preloaded <= kAudioRingBlocks; }
    bool enable() override { return true; }
    void disable() override {}
    bool write(const int16_t *) override {
        ++played_;
        return true;
    }
    uint32_t blocks_played() const override { return played_; }
    uint32_t underrun_events() const override { return 0; }
    uint32_t preloaded = 0;

  private:
    uint32_t played_ = 0;
};

// ===========================================================================
// Scenarios: the same command schedule, block by block, under different
// producer timing. The pump renders deterministically, so the sequence of
// blocks it hands the ring must not depend on timing at all.
// ===========================================================================
struct Scenario {
    uint32_t steps = 0;
    std::function<void(AudioRingPump &, uint32_t step)> commands;
    std::function<uint64_t(uint32_t step)> producer_us; // render time + any stall before this step
    std::function<uint16_t(uint32_t step)> sfx_gain = [](uint32_t) { return kUnityGainQ15; };
    std::function<uint16_t(uint32_t step)> music_gain = [](uint32_t) { return volume_to_gain_q15(80); };
    size_t ring_blocks = kAudioRingBlocks;
};

struct Outcome {
    VirtualI2sRing ring;
    AudioPerfSnapshot perf{};
    uint32_t yields = 0;
    std::vector<uint64_t> step_us; // ring time when step k's commands were applied
    explicit Outcome(size_t n) : ring(n) {}
};

SfxRequest sfx_request(SfxId id, uint32_t sequence) {
    SfxRequest r{};
    r.id = id;
    r.gain_q15 = kUnityGainQ15;
    r.sequence = sequence;
    return r;
}

void run(const Scenario &sc, const MusicLibrary *library, Outcome &out) {
    AudioRingPump pump(sc.ring_blocks);
    pump.set_music_library(library);
    pump.set_clock({&out.ring, &ring_clock});
    pump.reset_perf();
    for (uint32_t k = 0; k < sc.steps; ++k) {
        out.step_us.push_back(out.ring.now());
        if (sc.commands) sc.commands(pump, k);
        if (pump.sleeping()) {
            out.ring.advance(20000); // the device's bounded queue wait
            continue;
        }
        out.ring.advance(sc.producer_us ? sc.producer_us(k) : 0);
        if (pump.step(out.ring, sc.sfx_gain(k), sc.music_gain(k))) {
            ++out.yields;
            out.ring.advance(10000); // vTaskDelay(1) at CONFIG_FREERTOS_HZ=100
        }
    }
    pump.perf(out.perf);
}

/**
 * The DMA played every written block exactly once, in order -- except the
 * blocks still queued when the channel was turned off (by design only the
 * drain's silence, which R3 checks) and at most one ring still queued when
 * the run ends. No block skipped, repeated or reordered.
 */
bool played_in_order(const VirtualI2sRing &ring) {
    std::vector<uint32_t> expected;
    for (uint32_t id = 1; id <= ring.written.size(); ++id)
        if (std::find(ring.cut_ids.begin(), ring.cut_ids.end(), id) == ring.cut_ids.end()) expected.push_back(id);
    size_t e = 0;
    for (const auto &p : ring.played) {
        if (p.id == 0) continue;
        if (e >= expected.size() || p.id != expected[e]) return false;
        ++e;
    }
    return e > 0 && expected.size() - e <= ring.descriptors();
}
bool same_blocks(const VirtualI2sRing &a, const VirtualI2sRing &b) {
    const size_t n = std::min(a.written.size(), b.written.size());
    if (n == 0) return false;
    for (size_t i = 0; i < n; ++i)
        if (a.written[i] != b.written[i]) return false;
    return true;
}

// A rich schedule: title music, a burst of steps and hits, a song switch,
// music stopped (SFX alone, the channel drains and turns off), SFX from off,
// music again.
void rich_commands(AudioRingPump &p, uint32_t k) {
    static const SfxId kCues[] = {SfxId::MoveStep, SfxId::CombatHit, SfxId::MoveBlocked, SfxId::CombatHitHeavy};
    if (k == 0) p.play_music(MusicSong::Theme);
    if (k >= 40 && k < 1400 && k % 13 == 0) p.submit_sfx(sfx_request(kCues[(k / 13) % 4], k), 0);
    if (k == 700) p.play_music(MusicSong::BritannicLands);
    if (k == 1100) p.stop_music();
    if (k == 1500) p.submit_sfx(sfx_request(SfxId::CombatHit, k), 0);
    if (k == 1700) p.play_music(MusicSong::Engagement);
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: a3_04a_audio_stream <native/core dir> <openu5-audio.bin>\n");
        return 2;
    }
    const std::string core_dir = argv[1];
    const std::string device_dir = core_dir + "/../targets/tdeck/main";

    // ======================================================================
    // H -- the hot path
    // ======================================================================
    {
        // A function-local static object is guarded by __cxa_guard_acquire.
        // With -mdisable-hardware-atomics GCC cannot inline the "already
        // built?" test and calls it on EVERY access (a FreeRTOS mutex take +
        // give in ESP-IDF's cxx_guards.cpp): A3-04's tables() did that four
        // times per sounding channel per chip sample. `static constexpr` is
        // constant-initialized and never guarded; everything else is banned
        // from the files the audio task runs.
        const std::regex local_static(R"(^\s+static\s+(?!constexpr\b))");
        for (const char *file : {"/src/music_synth.cpp", "/src/sfx_synth.cpp", "/src/audio_stream.cpp", "/src/audio.cpp"}) {
            const std::string src = strip_comments(slurp(core_dir + file));
            std::istringstream in(src);
            std::string offender;
            for (std::string line; std::getline(in, line);)
                if (std::regex_search(line, local_static)) offender = line;
            check(!src.empty() && offender.empty(),
                  std::string("H1 no function-local static in the audio task's code: ") + file +
                      (offender.empty() ? "" : " -- found `" + offender + "`"));
        }
        const std::string synth = strip_comments(slurp(core_dir + "/src/music_synth.cpp"));
        const auto at = synth.find("const OplTables &tables()");
        const auto end = synth.find('}', at);
        check(at != std::string::npos && synth.substr(at, end - at).find("static") == std::string::npos,
              "H2 tables() returns a namespace-scope object: no guard on the synth's per-sample path");

        const std::string cmake = strip_comments(slurp(device_dir + "/CMakeLists.txt"));
        const std::regex o2(R"(set_source_files_properties\([^)]*music_synth\.cpp[^)]*COMPILE_OPTIONS[^)]*-O2[^)]*-ffp-contract=off)");
        check(std::regex_search(cmake, o2),
              "H3 the firmware builds music_synth.cpp at -O2 with FP contraction off (same IEEE results as -Og, "
              "no out-of-line call per operator)");

        const std::string stream = strip_comments(slurp(core_dir + "/src/audio_stream.cpp"));
        std::string found;
        for (const char *t : {"vTaskDelay", "xQueue", "xSemaphore", "esp_", "sleep(", "std::mutex", "ESP_LOG", "malloc",
                              "make_unique", "push_back", ".resize(", "new "})
            if (stream.find(t) != std::string::npos) found += std::string(" ") + t;
        check(!stream.empty() && found.empty(),
              "H4 audio_stream.cpp holds no RTOS call, lock, log or allocation" + (found.empty() ? "" : ":" + found));
    }

    // ======================================================================
    // L -- the music library
    // ======================================================================
    const auto pack = slurp_bytes(argv[2]);
    AudioPackPayload payload{};
    const AudioPackInfo info = inspect_audio_pack(pack.data(), pack.size(), &payload);
    MusicLibrary library;
    {
        check(!pack.empty() && info.state == AudioPackState::Valid &&
                  info.record.capability == MusicCapability::SupportedMusicPatch,
              "L0 the real audio pack is present, Valid and Supported (run: npm run pack:audio)");
        const size_t playable = library.load(&payload);
        check(playable == 16 && library.loaded() && library.bank().count() == 181,
              "L1 the library parses all 16 songs and the 181-timbre bank once, at load");
        bool same = true;
        for (size_t s = 0; s < kMusicSongCount; ++s) {
            MusicTrack direct;
            const bool ok = parse_xmi_events(payload.song[s], payload.song_length[s], direct);
            const MusicTrack *t = library.track(MusicSong(s));
            same = same && ok && t && t->events.size() == direct.events.size() && t->end_tick == direct.end_tick;
        }
        check(same, "L2 every resident track is the song's own parse (same events, same end tick)");
        check(library.event_count() == 21529,
              "L3 the resident corpus is 21,529 events (172 KB at 8 bytes; measured in section 18.4)");
        MusicLibrary empty;
        AudioPackPayload no_bank = payload;
        no_bank.bank = nullptr;
        MusicLibrary bankless;
        check(empty.load(nullptr) == 0 && !empty.track(MusicSong::Theme) && bankless.load(&no_bank) == 0 &&
                  !bankless.track(MusicSong::Theme) && !library.track(MusicSong::None),
              "L4 no payload, no bank, or MusicSong::None: nothing playable, never a guess");
    }

    // ======================================================================
    // P -- the counters' arithmetic
    // ======================================================================
    {
        AudioPerfCounters c;
        c.reset(1000);
        // 100 blocks: 1..100 % of a block (80 us steps), plus one 150 % block.
        for (uint32_t i = 1; i <= 100; ++i) c.on_render(i * kAudioBlockUs / 100, i * 40, i % 10, 9, i % 3);
        c.on_render(kAudioBlockUs * 3 / 2, 5000, 4, 7, 0);
        c.on_write(8, 100, 0, true, false);
        c.on_write(3, 7000, 8000, true, true);
        c.on_write(0, 0, 20000, false, true);
        c.on_underrun();
        c.on_hw_underruns(2);
        c.on_sfx(true);
        c.on_sfx(false);
        c.on_sfx_queue_depth(3);
        c.on_sfx_queue_depth(1);
        c.on_music_switch();
        AudioPerfSnapshot s{};
        s.seq = 42;
        c.snapshot(1000 + 2000000, s);
        const uint32_t sum = 5050 * kAudioBlockUs / 100 + kAudioBlockUs * 3 / 2;
        check(s.seq == 42 && s.window_us == 2000000 && s.blocks == 101 && s.render_min_us == kAudioBlockUs / 100 &&
                  s.render_max_us == kAudioBlockUs * 3 / 2 && s.render_avg_us == sum / 101,
              "P1 render min/avg/max over the window; the snapshot keeps the publisher's seq");
        check(s.render_p95_us == 97 * kAudioBlockUs / 100 && s.render_p99_us == 101 * kAudioBlockUs / 100,
              "P2 p95/p99 = the upper edge of the 1 %-of-a-block bucket holding rank 96 / 100 of 101 (97 %, 101 %)");
        check(s.missed_deadlines == 1, "P3 exactly the one block that took longer than it lasts is a missed deadline");
        check(s.fill_min == 0 && s.fill_max == 8 && s.fill_avg_x100 == 366 && s.write_max_us == 7000 &&
                  s.write_failures == 1 && s.written == 2 && s.period_max_us == 20000 && s.period_avg_us == 14000,
              "P4 fill, write and period statistics (the first write has no period)");
        check(s.cpu_permille == uint32_t(uint64_t(sum) * 1000 / 2000000) && s.underruns == 1 && s.hw_underruns == 2 &&
                  s.sfx_submitted == 2 && s.sfx_during_music == 1 && s.sfx_queue_max == 3 && s.music_switches == 1 &&
                  s.channels_max == 9 && s.voices_max == 9 && s.sfx_pending_max == 2,
              "P5 CPU = render time over the window; underrun/SFX/switch/voice counters");
        c.reset(5000);
        AudioPerfSnapshot z{};
        c.snapshot(5000, z);
        check(z.blocks == 0 && z.render_max_us == 0 && z.fill_min == 0 && z.underruns == 0 && z.missed_deadlines == 0,
              "P6 a reset starts an empty window");
        // What A3-04's guard cost per block, at 1 us per guarded read and the
        // Theme's measured 8.76 sounding channels: ~13.9 ms of every 8 ms block.
        const uint32_t legacy = legacy_guard_us_per_block(1000, 876);
        check(legacy == uint32_t(4ull * 876 * kAudioBlockFrames * kOplClockHz / kSfxOutputRateHz * 1000 / 100 / 1000) &&
                  legacy > kAudioBlockUs,
              "P7 the legacy-guard estimate: 4 reads x sounding channels x chip samples per block x the guard's cost");
    }

    // ======================================================================
    // R -- the pump against the driver model
    // ======================================================================
    const uint64_t kRender = kAudioBlockUs / 2; // a producer at 50 % of real time
    auto steady = [&](uint32_t) { return kRender; };

    // The reference: the rich schedule with no stall at all.
    Scenario rich;
    rich.steps = 2000;
    rich.commands = rich_commands;
    rich.producer_us = steady;
    Outcome ref(kAudioRingBlocks);
    run(rich, &library, ref);
    {
        const auto &r = ref.ring;
        check(r.enables >= 2 && r.preloads == r.enables * kAudioRingBlocks && r.disables == r.enables - 1,
              "R1 every start from silence preloads EVERY descriptor before enabling (" + std::to_string(r.enables) +
                  " starts, " + std::to_string(r.preloads) + " preloads)");
        bool first_ring_fresh = r.played.size() >= kAudioRingBlocks;
        for (size_t i = 0; first_ring_fresh && i < kAudioRingBlocks; ++i) first_ring_fresh = r.played[i].id == uint32_t(i + 1);
        check(first_ring_fresh, "R1 the first ring the DMA plays is exactly the first eight rendered blocks (no lead-in)");
        check(r.stale_plays == 0, "R2 no stale replay: after the off/on cycle the DMA never plays a previous sound's block");
        check(r.cut_real_blocks == 0, "R3 the drain plays every real block before the clocks stop (no cut tail)");
        check(r.dry_plays == 0 && ref.perf.hw_underruns == 0 && ref.perf.underruns == 0,
              "R4 at 50 % load with no stall: zero empty blocks, zero driver overflows, zero dry writes");
        check(played_in_order(r), "R4 every block played in order: none skipped, repeated or reordered");
        check(ref.perf.fill_min >= kAudioRingBlocks - 1 && ref.yields == 0,
              "R4 the ring stays full (fill never below 7 of 8) and the task never needs a runaway yield");
    }

    // The oracle: the pump's stream IS the two players rendered directly,
    // block for block -- an independent reference, so a deterministic pump
    // bug (a block rendered and dropped, a gain on the wrong channel, a
    // replacing instead of summing mix) cannot hide behind a self-comparison.
    {
        const uint16_t mg = volume_to_gain_q15(80), sg = kUnityGainQ15;
        auto cue = [](uint32_t k) { return k % 2 ? SfxId::CombatHit : SfxId::MoveStep; };
        Scenario sc;
        sc.steps = 600;
        sc.producer_us = steady;
        sc.commands = [&](AudioRingPump &p, uint32_t k) {
            if (k == 0) p.play_music(MusicSong::Theme);
            if (k % 17 == 5) p.submit_sfx(sfx_request(cue(k), k), 0);
        };
        Outcome o(kAudioRingBlocks);
        run(sc, &library, o);
        MusicSongPlayer music;
        music.start(*library.track(MusicSong::Theme), library.bank(), kDeviceMusicChip, /*loop=*/true);
        SfxPlayer sfx;
        int16_t mb[kAudioBlockFrames], sb[kAudioBlockFrames];
        bool same = o.ring.written.size() == sc.steps;
        size_t sfx_blocks = 0;
        for (uint32_t k = 0; same && k < sc.steps; ++k) {
            if (k % 17 == 5) sfx.submit(sfx_request(cue(k), k), 0);
            music.render(mb, kAudioBlockFrames, kSfxOutputRateHz, mg);
            if (sfx.render(sb, kAudioBlockFrames, sg) > 0) ++sfx_blocks;
            for (size_t i = 0; i < kAudioBlockFrames; ++i) {
                const int32_t sum = std::max(-32768, std::min(32767, int32_t(mb[i]) + int32_t(sb[i])));
                same = same && o.ring.written[k][i] == int16_t(sum);
            }
        }
        check(same && sfx_blocks > 50,
              "R17 the oracle: 600 blocks of the Theme + cues equal MusicSongPlayer and SfxPlayer rendered directly "
              "and summed -- nothing skipped, repeated, re-gained or replaced");
    }

    // Stall matrix: one stall of S ms in the middle of the music + SFX section.
    uint32_t tolerated_ms = 0;
    {
        for (uint32_t stall_ms : {5u, 10u, 20u, 40u}) {
            Scenario sc = rich;
            sc.producer_us = [=](uint32_t k) { return kRender + (k == 500 ? uint64_t(stall_ms) * 1000 : 0); };
            Outcome o(kAudioRingBlocks);
            run(sc, &library, o);
            check(o.ring.dry_plays == 0 && o.perf.hw_underruns == 0 && o.perf.underruns == 0 && same_blocks(o.ring, ref.ring) &&
                      played_in_order(o.ring),
                  "R5 a " + std::to_string(stall_ms) + " ms stall: no underrun, and the stream is the reference, block for block");
        }
        Scenario sc = rich;
        sc.producer_us = [=](uint32_t k) { return kRender + (k == 500 ? 100000u : 0); };
        Outcome o(kAudioRingBlocks);
        run(sc, &library, o);
        const uint32_t dry = o.ring.dry_plays, hw = o.perf.hw_underruns;
        check(dry > 0 && (hw == dry || hw == dry + 1) && o.perf.underruns >= 1,
              "R5 a 100 ms stall: the speaker runs dry (" + std::to_string(dry) + " empty blocks) and BOTH detectors see it "
              "(driver " + std::to_string(hw) + ", writer " + std::to_string(o.perf.underruns) + ")");
        check(same_blocks(o.ring, ref.ring) && played_in_order(o.ring) && o.ring.stale_plays == 0,
              "R5 ... and recovers: nothing skipped or repeated, the same blocks in the same order after the gap");

        // Designed tolerance: the longest single stall with no empty block.
        for (uint32_t s = 0; s <= 120; ++s) {
            Scenario t = rich;
            t.steps = 900;
            t.producer_us = [=](uint32_t k) { return kRender + (k == 500 ? uint64_t(s) * 1000 : 0); };
            Outcome u(kAudioRingBlocks);
            run(t, &library, u);
            if (u.ring.dry_plays != 0) break;
            tolerated_ms = s;
        }
        const uint32_t designed = (kAudioRingBlocks * kAudioBlockUs - uint32_t(kRender)) / 1000;
        std::printf("  designed stall tolerance at 50 %% load: %u ms (ring %u x %u us, render %u us)\n", tolerated_ms,
                    unsigned(kAudioRingBlocks), unsigned(kAudioBlockUs), unsigned(kRender));
        check(tolerated_ms + 1 >= designed && tolerated_ms <= designed,
              "R6 stall tolerance = the whole ring minus one block's render: " + std::to_string(tolerated_ms) + " ms (>= 40)");
        check(tolerated_ms >= 40, "R6 ... which covers the 40 ms case with margin");

        // Repeated jitter: a random 0-40 ms stall, at random, never closer
        // than 0.3 s to the previous one (the ring refills at +4 ms per block
        // at this load, so back-to-back 40 ms stalls are a different claim --
        // R6's single-stall tolerance bounds those), plus up to 2 ms of
        // render jitter on every block.
        Scenario j = rich;
        uint32_t lcg = 12345, stalls = 0, last_stall = 0;
        std::vector<uint64_t> jitter(j.steps);
        for (uint32_t k = 0; k < j.steps; ++k) {
            lcg = lcg * 1103515245u + 12345u;
            uint64_t v = kRender + (lcg >> 8) % 2001;
            if (k >= last_stall + 38 && ((lcg >> 20) % 29) == 0) {
                v += uint64_t((lcg >> 12) % 41) * 1000;
                last_stall = k;
                ++stalls;
            }
            jitter[k] = v;
        }
        j.producer_us = [&](uint32_t k) { return jitter[k]; };
        Outcome jo(kAudioRingBlocks);
        run(j, &library, jo);
        std::printf("  random jitter: %u stalls of 0-40 ms over %u blocks: %u empty blocks, fill min %u\n", stalls,
                    unsigned(j.steps), jo.ring.dry_plays, jo.perf.fill_min);
        check(stalls >= 20 && jo.ring.dry_plays == 0 && same_blocks(jo.ring, ref.ring) && played_in_order(jo.ring),
              "R7 random stalls up to 40 ms all through music, SFX and a song switch: no underrun, exact stream");

        // Geometry: a shallower ring tolerates less (the "ring too shallow" case).
        Scenario shallow = rich;
        shallow.ring_blocks = 3;
        shallow.producer_us = [=](uint32_t k) { return kRender + (k == 500 ? 40000u : 0); };
        Outcome so(3);
        run(shallow, &library, so);
        check(so.ring.dry_plays > 0 && so.perf.hw_underruns > 0,
              "R8 the same 40 ms stall DOES underrun a 3-descriptor ring (24 ms): depth is what buys the tolerance, and it is measured");
    }

    // A3-04's condition: the producer slower than real time.
    {
        Scenario sc = rich;
        sc.steps = 400;
        sc.producer_us = [](uint32_t) { return uint64_t(kAudioBlockUs) * 3 / 2; };
        Outcome o(kAudioRingBlocks);
        run(sc, &library, o);
        check(o.ring.dry_plays > 100 && o.perf.hw_underruns > 100 && o.perf.underruns > 0,
              "R9 a producer at 150 % of real time (A3-04's synth, section 18.3) underruns continuously -- counted by both "
              "detectors (" + std::to_string(o.perf.hw_underruns) + " driver overflows)");
        check(o.yields > 0 && o.perf.runaway_yields == o.yields,
              "R9 ... and the runaway guard yields the core (" + std::to_string(o.yields) + " times) instead of starving its idle task");
    }

    // Song switch: bounded latency, and nothing of the old song after it.
    {
        Scenario a;
        a.steps = 400;
        a.producer_us = steady;
        a.commands = [](AudioRingPump &p, uint32_t k) {
            if (k == 0) p.play_music(MusicSong::Theme);
            if (k == 300) p.play_music(MusicSong::Stones);
        };
        Outcome o(kAudioRingBlocks);
        run(a, &library, o);
        Scenario b;
        b.steps = 100;
        b.producer_us = steady;
        b.commands = [](AudioRingPump &p, uint32_t k) {
            if (k == 0) p.play_music(MusicSong::Stones);
        };
        Outcome fresh(kAudioRingBlocks);
        run(b, &library, fresh);
        // Step 300's block is written block #301 (steps 0..7 primed 8, then one per step).
        bool match = o.ring.written.size() >= 390 && fresh.ring.written.size() >= 90;
        for (size_t i = 0; match && i < 90; ++i) match = o.ring.written[300 + i] == fresh.ring.written[i];
        check(match, "R10 the block rendered right after a switch is the new song's first block, and the next 90 are "
                     "exactly a fresh start of it (nothing of the old song leaks through the player)");
        uint64_t heard_us = 0;
        for (const auto &p : o.ring.played)
            if (p.id == 301) heard_us = p.start_us;
        const uint64_t latency = heard_us > o.step_us[300] ? heard_us - o.step_us[300] : 0;
        std::printf("  song switch: command -> first new block at the speaker = %.1f ms\n", double(latency) / 1000.0);
        check(heard_us > 0 && latency <= uint64_t(kAudioRingBlocks + 1) * kAudioBlockUs,
              "R10 the new song reaches the speaker within one ring plus one block (72 ms) of the command");
    }

    // Volume: bounded, and never retroactive.
    {
        auto music_only = [](AudioRingPump &p, uint32_t k) {
            if (k == 0) p.play_music(MusicSong::Theme);
        };
        Scenario lo, hi, change;
        for (Scenario *s : {&lo, &hi, &change}) {
            s->steps = 300;
            s->producer_us = steady;
            s->commands = music_only;
        }
        lo.music_gain = [](uint32_t) { return volume_to_gain_q15(80); };
        hi.music_gain = [](uint32_t) { return volume_to_gain_q15(30); };
        change.music_gain = [](uint32_t k) { return volume_to_gain_q15(k < 200 ? 80 : 30); };
        Outcome ol(kAudioRingBlocks), oh(kAudioRingBlocks), oc(kAudioRingBlocks);
        run(lo, &library, ol);
        run(hi, &library, oh);
        run(change, &library, oc);
        // Step 200 renders written block #201 (index 200).
        const size_t n = oc.ring.written.size();
        bool before = n > 200 && ol.ring.written.size() == n && oh.ring.written.size() == n, after = before;
        for (size_t i = 0; before && i < 200; ++i) before = oc.ring.written[i] == ol.ring.written[i];
        for (size_t i = 200; after && i < n; ++i) after = oc.ring.written[i] == oh.ring.written[i];
        check(before && after,
              "R11 a Music Volume change reaches the very next block and no block already in the ring (volume is per block)");
    }

    // SFX over music: a saturating sum, and each gain on its own channel only.
    {
        auto sfx_schedule = [](AudioRingPump &p, uint32_t k) {
            if (k % 9 == 0) p.submit_sfx(sfx_request(k % 2 ? SfxId::CombatHit : SfxId::MoveStep, k), 0);
        };
        Scenario both, music, sfx;
        for (Scenario *s : {&both, &music, &sfx}) {
            s->steps = 400;
            s->producer_us = steady;
        }
        both.commands = [&](AudioRingPump &p, uint32_t k) {
            if (k == 0) p.play_music(MusicSong::Engagement);
            sfx_schedule(p, k);
        };
        music.commands = [&](AudioRingPump &p, uint32_t k) {
            if (k == 0) p.play_music(MusicSong::Engagement);
            sfx_schedule(p, k); // at SFX Volume 0
        };
        sfx.commands = [&](AudioRingPump &p, uint32_t k) {
            if (k == 0) p.play_music(MusicSong::Engagement);
            sfx_schedule(p, k);
        };
        music.sfx_gain = [](uint32_t) { return uint16_t(0); };
        sfx.music_gain = [](uint32_t) { return uint16_t(0); };
        Scenario pure;
        pure.steps = 400;
        pure.producer_us = steady;
        pure.commands = [](AudioRingPump &p, uint32_t k) {
            if (k == 0) p.play_music(MusicSong::Engagement);
        };
        Outcome ob(kAudioRingBlocks), om(kAudioRingBlocks), os(kAudioRingBlocks), op(kAudioRingBlocks);
        run(both, &library, ob);
        run(music, &library, om);
        run(sfx, &library, os);
        run(pure, &library, op);
        check(same_blocks(om.ring, op.ring) && om.ring.written.size() == op.ring.written.size(),
              "R12 SFX Volume 0: cues submitted at gain 0 leave the music stream bit-identical to no cues at all");
        bool summed = ob.ring.written.size() == om.ring.written.size() && om.ring.written.size() == os.ring.written.size();
        bool overlapped = false;
        for (size_t b = 0; summed && b < ob.ring.written.size(); ++b)
            for (size_t i = 0; i < kAudioBlockFrames; ++i) {
                const int32_t m = om.ring.written[b][i], s = os.ring.written[b][i];
                const int32_t sum = std::max(-32768, std::min(32767, m + s));
                summed = summed && ob.ring.written[b][i] == int16_t(sum);
                overlapped = overlapped || (m != 0 && s != 0);
            }
        check(summed && overlapped,
              "R12 SFX overlay music as a saturating sum, sample for sample (SFX Volume 0 = music alone, Music Volume 0 = "
              "SFX alone: each gain touches its own channel only)");
    }

    // Stop -> drain -> off; then silence costs nothing.
    {
        Scenario sc;
        sc.steps = 200;
        sc.producer_us = steady;
        bool slept = false;
        sc.commands = [&](AudioRingPump &p, uint32_t k) {
            if (k == 0) p.play_music(MusicSong::Theme);
            if (k == 100) p.stop_music();
            if (k == 150) slept = p.sleeping() && p.state() == AudioRingPump::State::Off;
        };
        Outcome o(kAudioRingBlocks);
        run(sc, &library, o);
        size_t last_real = 0;
        for (size_t i = 0; i < o.ring.written.size(); ++i)
            if (std::any_of(o.ring.written[i].begin(), o.ring.written[i].end(), [](int16_t v) { return v != 0; })) last_real = i;
        check(o.ring.disables == 1 && o.ring.written.size() == last_real + 1 + kAudioRingBlocks && o.ring.cut_real_blocks == 0 &&
                  slept,
              "R13 after the music stops, exactly one ring of silence is written, the clocks stop, and the task sleeps");
    }

    // Enable failure: silent, counted, yields.
    {
        VirtualI2sRing ring(kAudioRingBlocks);
        ring.fail_enable = true;
        AudioRingPump pump;
        pump.set_music_library(&library);
        pump.play_music(MusicSong::Theme);
        bool yielded = false;
        for (uint32_t k = 0; k < kAudioRingBlocks; ++k) yielded = pump.step(ring, kUnityGainQ15, kUnityGainQ15) || yielded;
        AudioPerfSnapshot s{};
        pump.perf(s);
        check(yielded && pump.state() == AudioRingPump::State::Off && !pump.music().active() && s.enable_failures == 1,
              "R14 a failed I2S enable leaves the audio path silent and off, counted, and yields instead of spinning");
    }

    // Transport: a request posted before a flush is stale and not counted.
    {
        AudioRingPump pump;
        pump.sync_epoch(1);
        const auto stale = pump.submit_sfx(sfx_request(SfxId::MoveStep, 1), 0);
        const auto fresh = pump.submit_sfx(sfx_request(SfxId::MoveStep, 2), 1);
        AudioPerfSnapshot s{};
        pump.perf(s);
        check(stale == SfxAdmit::Stale && fresh == SfxAdmit::Started && s.sfx_submitted == 1,
              "R15 the flush epoch still drops in-flight requests; only admitted cues are counted");
    }

    // Zero allocation per block and per song switch.
    {
        CountingSink sink;
        AudioRingPump pump;
        pump.set_music_library(&library);
        pump.play_music(MusicSong::Theme);
        pump.step(sink, kUnityGainQ15, kUnityGainQ15); // first render sizes the chip-rate buffer once
        const long before = g_alloc_count;
        for (uint32_t k = 0; k < 3000; ++k) {
            if (k % 11 == 0) pump.submit_sfx(sfx_request(SfxId::MoveStep, k), 0);
            if (k == 500) pump.play_music(MusicSong::Stones);
            if (k == 1000) pump.play_music(MusicSong::Engagement);
            if (k == 1500) pump.stop_music();
            if (k == 1600) pump.play_music(MusicSong::HallsOfDoom);
            pump.step(sink, kUnityGainQ15, volume_to_gain_q15(50));
        }
        const long allocated = g_alloc_count - before; // read before check() builds its label string
        AudioPerfSnapshot s{};
        pump.perf(s);
        check(allocated == 0 && s.music_switches == 4,
              "R16 3,000 blocks, SFX every 11 and four song switches after warm-up: zero heap allocations");
    }

    // ======================================================================
    // B -- the Developer benchmark
    // ======================================================================
    {
        AudioBenchmark b;
        b.start(1000);
        uint32_t resets = 0, idle = 0, stress = 0, finished = 0, cues = 0, idle_at = 0, stress_at = 0;
        for (uint32_t t = 1000; t <= 1000 + 50000; t += 7) {
            const auto a = b.tick(t);
            resets += a.reset_perf;
            if (a.capture_idle) {
                ++idle;
                idle_at = t - 1000;
            }
            if (a.capture_stress) {
                ++stress;
                stress_at = t - 1000;
            }
            finished += a.finished;
            cues += a.sfx != SfxId::None;
        }
        check(resets == 2 && idle == 1 && stress == 1 && finished == 1 && !b.running(),
              "B1 settle -> reset; 30 s music alone -> read + reset; 15 s music + SFX -> read + restore; then done");
        check(idle_at >= 32000 && idle_at < 32010 && stress_at >= 47000 && stress_at < 47020,
              "B1 ... at 2 s + 30 s and + 15 s");
        check(cues >= 149 && cues <= 151 && b.sfx_played() == cues, "B2 one cue every 100 ms during the SFX phase (~150)");
        AudioBenchmark late;
        late.start(0);
        late.tick(2000);
        late.tick(32000);
        const auto burst = late.tick(40000); // a frame 8 s late
        const auto next = late.tick(40001);
        check(burst.sfx != SfxId::None && next.sfx == SfxId::None,
              "B3 a late frame plays ONE cue, never a catch-up burst");

        AudioPerfSnapshot s{};
        s.window_us = 30000000;
        s.render_avg_us = 3210;
        s.render_p99_us = 3900;
        s.render_max_us = 4700;
        s.fill_min = 7;
        s.period_max_us = 8400;
        s.music_active = true;
        s.song = MusicSong::Theme;
        s.channels_avg_x100 = 876;
        s.channels_max = 9;
        char lines[10][64]{};
        const size_t n = format_audio_perf(s, lines, 10);
        std::string all;
        bool short_enough = n >= 7;
        for (size_t i = 0; i < n; ++i) {
            short_enough = short_enough && std::strlen(lines[i]) <= 63;
            all += std::string(lines[i]) + "\n";
        }
        std::printf("%s", all.c_str());
        check(short_enough && all.find("missed 0") != std::string::npos && all.find("render avg 3.21 p99 3.90 max 4.70") != std::string::npos &&
                  all.find("min buffered 56 ms") != std::string::npos && all.find("sched max 8.40") != std::string::npos &&
                  all.find("OPL ch avg 8.7 max 9") != std::string::npos,
              "B4 the compact report: missed/underrun, render avg/p99/max, sched max, min buffered, voices");
        char one[1][64]{};
        check(format_audio_perf(s, one, 1) == 1, "B4 ... and never writes past the caller's lines");
    }

    // ======================================================================
    // S -- the device backend's wiring
    // ======================================================================
    {
        const std::string device = strip_comments(slurp(device_dir + "/tdeck_audio.cpp"));
        const std::string header = strip_comments(slurp(device_dir + "/tdeck_audio.h"));
        check(device.find("pump_.step(") != std::string::npos && header.find("openu5::AudioRingPump pump_") != std::string::npos,
              "S1 the device audio task runs the production pump this test drives");
        check(device.find("dma_desc_num = openu5::kAudioRingBlocks") != std::string::npos &&
                  device.find("dma_frame_num = openu5::kAudioBlockFrames") != std::string::npos,
              "S2 the DMA ring is exactly the geometry the pump and this model assume");
        check(std::regex_search(device, std::regex(R"(i2s_channel_register_event_callback\(tx_)")) &&
                  device.find(".on_sent") != std::string::npos && device.find(".on_send_q_ovf") != std::string::npos,
              "S3 the driver's on_sent / on_send_q_ovf callbacks feed blocks_played() and underrun_events()");
        const auto run_at = device.find("void TdeckAudioBackend::run()");
        const std::string run_body = run_at == std::string::npos ? "" : device.substr(run_at, device.find("\n}\n", run_at) - run_at);
        check(!run_body.empty() && run_body.find("parse_xmi_events") == std::string::npos &&
                  run_body.find("ESP_LOG") == std::string::npos && device.find("library_.load(") != std::string::npos,
              "S4 songs are parsed once at boot (MusicLibrary::load); the audio task never parses, and never logs");
    }

    std::printf("A3-04A audio stream: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
