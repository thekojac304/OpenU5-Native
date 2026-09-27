// Alpha 3 A3-04C -- the contention map (ALPHA3_AUDIO.md section 20). Host side:
//
//   P  the Developer "synth bypass" probe on the production pump: the song
//      stays playing (channel, DMA cadence, SFX) but the synth does no work
//      and the song does not advance; switching it off resumes the song
//      exactly where it stopped
//   B  the audio task's own work per block (delivery interval minus the
//      write's wait), against the ESP-IDF descriptor-ring model
//   C  the contention counters' arithmetic (the TFT split, the row-level
//      audio-busy/idle classification, viewport vs other frames, yields,
//      slow transactions, the loop)
//   F  the Developer report's contention section: every field, every line
//      fits a Developer row, nothing cut off the end of the longest report
//   L  the one-line A3C_PERF heartbeat: every field in a fixed order, bounded
//   S  the device-only wiring (source scans): every TFT transaction and
//      draw-loop yield is timed, the audio task's flag brackets its waits,
//      main.cpp attaches all of it
//
//   a3_04c_contention <native/core dir> <native/assets/openu5-audio.bin>
#include "openu5/audio.h"
#include "openu5/audio_pack.h"
#include "openu5/audio_stream.h"
#include "openu5/music_synth.h"
#include "openu5/perf_report.h"
#include "openu5/sfx_synth.h"
#include "a3_04a_virtual_i2s_ring.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace openu5;
using openu5_test::ring_clock;
using openu5_test::VirtualI2sRing;

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
size_t count_of(const std::string &hay, const std::string &needle) {
    size_t n = 0;
    for (size_t at = hay.find(needle); at != std::string::npos; at = hay.find(needle, at + needle.size())) ++n;
    return n;
}
/** The body of the first function whose definition line contains `signature`. */
std::string function_body(const std::string &src, const std::string &signature) {
    const auto at = src.find(signature);
    if (at == std::string::npos) return {};
    const auto open = src.find('{', at);
    if (open == std::string::npos) return {};
    int depth = 0;
    for (size_t i = open; i < src.size(); ++i) {
        if (src[i] == '{') ++depth;
        else if (src[i] == '}' && --depth == 0) return src.substr(open, i - open + 1);
    }
    return {};
}

SfxRequest sfx_request(SfxId id, uint32_t sequence) {
    SfxRequest r{};
    r.id = id;
    r.gain_q15 = kUnityGainQ15;
    r.sequence = sequence;
    return r;
}

// Plays one block per write and records every call: each step renders one block.
class CountingSink final : public PcmRingSink {
  public:
    bool preload(const int16_t *) override { return ++preloaded <= kAudioRingBlocks; }
    bool enable() override {
        ++enables;
        return true;
    }
    void disable() override { ++disables; }
    bool write(const int16_t *) override {
        ++writes;
        return true;
    }
    uint32_t blocks_played() const override { return writes; }
    uint32_t underrun_events() const override { return 0; }
    uint32_t preloaded = 0, enables = 0, disables = 0, writes = 0;
};

bool same_block(const int16_t *a, const std::vector<int16_t> &b) {
    return std::equal(b.begin(), b.end(), a);
}
bool silent_block(const int16_t *a) {
    return std::all_of(a, a + kAudioBlockFrames, [](int16_t v) { return v == 0; });
}

std::string join(char (*lines)[kPerfReportLineBytes], size_t n) {
    std::string out;
    for (size_t i = 0; i < n; ++i) out += std::string(lines[i]) + "\n";
    return out;
}

// One frame's TFT timing as the device Board would measure it (cycles at 240 MHz).
TftTiming frame_timing(uint32_t xfer_us, uint32_t yield_us, bool viewport) {
    TftTiming t{};
    t.cpu_mhz = 240;
    t.transactions = 200;
    t.rows = 180;
    t.xfer_cycles = uint64_t(xfer_us) * 240;
    t.xfer_max_cycles = 1200 * 240; // one 1.2 ms transaction: slow
    t.slow_xfers = 1;
    t.slow_xfers_busy = 1;
    t.yields = 10;
    t.yield_cycles = uint64_t(yield_us) * 240;
    t.yield_max_cycles = 12000 * 240; // one late yield
    t.late_yields = 1;
    t.rows_idle = 100;
    t.rows_busy = 50;
    t.fill_cycles_idle = 100ull * 240 * 123 / 10; // 12.3 us a row
    t.fill_cycles_busy = 50ull * 240 * 150 / 10;  // 15.0 us a row
    t.xfer_cycles_idle = 100ull * 240 * 65;       // 65.0 us a row
    t.xfer_cycles_busy = 50ull * 240 * 66;        // 66.0 us a row
    t.viewport_full = viewport ? 1 : 0;
    return t;
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: a3_04c_contention <native/core dir> <openu5-audio.bin>\n");
        return 2;
    }
    const std::string core_dir = argv[1];
    const std::string device_dir = core_dir + "/../targets/tdeck";
    const std::string pack_bytes = slurp(argv[2]);
    std::vector<uint8_t> pack(pack_bytes.begin(), pack_bytes.end());
    AudioPackPayload payload{};
    const AudioPackInfo info = inspect_audio_pack(pack.data(), pack.size(), &payload);
    check(!pack.empty() && info.state == AudioPackState::Valid &&
              info.record.capability == MusicCapability::SupportedMusicPatch,
          "P0 the real audio pack is present, Valid and Supported (run: npm run pack:audio)");
    MusicLibrary library;
    library.load(&payload);
    const uint16_t gain = volume_to_gain_q15(80);

    // ======================================================================
    // P -- the synth bypass probe (section 20.4)
    // ======================================================================
    {
        AudioRingPump fresh;
        AudioPerfSnapshot s0{};
        fresh.perf(s0);
        check(!fresh.music_bypass() && !s0.music_bypass, "P1 the probe is off in a new pump (off at boot)");

        // The song's own blocks, from a player nobody interrupts.
        MusicSongPlayer ref;
        ref.start(*library.track(MusicSong::Theme), library.bank(), kDeviceMusicChip, true);
        std::vector<std::vector<int16_t>> expect(80, std::vector<int16_t>(kAudioBlockFrames));
        for (auto &b : expect) ref.render(b.data(), kAudioBlockFrames, kSfxOutputRateHz, gain);

        AudioRingPump pump;
        pump.set_music_library(&library);
        CountingSink sink;
        pump.play_music(MusicSong::Theme);
        bool before_ok = true;
        for (size_t i = 0; i < 40; ++i) {
            pump.step(sink, 0, gain);
            before_ok = before_ok && same_block(pump.last_block(), expect[i]);
        }
        check(before_ok && pump.state() == AudioRingPump::State::Running,
              "P2 probe off: the pump plays the Theme's own blocks (40 blocks = the uninterrupted player's)");

        pump.set_music_bypass(true);
        bool all_silent = true, running = true;
        const uint32_t writes_before = sink.writes;
        for (size_t i = 0; i < 30; ++i) {
            pump.step(sink, 0, gain);
            all_silent = all_silent && silent_block(pump.last_block());
            running = running && pump.state() == AudioRingPump::State::Running;
        }
        AudioPerfSnapshot s1{};
        pump.perf(s1);
        check(all_silent && pump.music().active() && pump.song() == MusicSong::Theme && s1.music_bypass &&
                  s1.music_active,
              "P3 probe on: 30 blocks of silence while the Theme stays the playing song (the report says BYPASS)");
        check(running && sink.writes == writes_before + 30 && sink.disables == 0 && pump.has_work() &&
                  !pump.sleeping(),
              "P4 ... and the channel keeps running: one write per block, never disabled (the DMA, its interrupt "
              "and the task's cadence are exactly those of music on)");

        // A cue during the probe is heard: SFX are not bypassed.
        pump.submit_sfx(sfx_request(SfxId::MoveBlocked, 1), 0);
        bool heard = false;
        for (size_t i = 0; i < 10; ++i) {
            pump.step(sink, kUnityGainQ15, gain);
            heard = heard || !silent_block(pump.last_block());
        }
        check(heard, "P5 a cue during the probe still sounds (SFX are rendered and mixed as before)");
        for (size_t i = 0; i < 40 && !pump.sfx().idle(); ++i) pump.step(sink, kUnityGainQ15, gain);
        check(pump.sfx().idle(), "P5 ... and finishes");

        pump.set_music_bypass(false);
        bool after_ok = true;
        for (size_t i = 40; i < 80; ++i) {
            pump.step(sink, 0, gain);
            after_ok = after_ok && same_block(pump.last_block(), expect[i]);
        }
        check(after_ok,
              "P6 probe off again: the song resumes exactly where it stopped -- blocks 41..80 of the uninterrupted "
              "player (the synth did not advance, and nothing in it was disturbed)");

        // No song: the probe creates no work, and the stop path is untouched.
        AudioRingPump idle;
        idle.set_music_library(&library);
        idle.set_music_bypass(true);
        CountingSink quiet;
        const bool yielded = idle.step(quiet, 0, gain);
        check(idle.sleeping() && !yielded && quiet.writes == 0 && quiet.preloaded == 0,
              "P7 with no song the probe starts nothing: the pump still sleeps");
        idle.play_music(MusicSong::Theme);
        for (int i = 0; i < 12; ++i) idle.step(quiet, 0, gain);
        idle.stop_music();
        int drained = 0;
        while (!idle.sleeping() && drained < 40) {
            idle.step(quiet, 0, gain);
            ++drained;
        }
        check(idle.sleeping() && quiet.disables == 1,
              "P8 Music Volume 0 % (stop) under the probe still drains the ring and turns the channel off (" +
                  std::to_string(drained) + " steps)");
    }

    // ======================================================================
    // B -- the audio task's own work per block (section 20.3)
    // ======================================================================
    {
        AudioPerfCounters c;
        c.reset(0);
        c.on_write(8, 5000, 0, true, false);    // the first write: no interval yet
        c.on_write(8, 5000, 8000, true, true);  // 3 ms of work
        c.on_write(8, 7000, 8000, true, true);  // 1 ms of work
        c.on_write(8, 9000, 8000, true, true);  // a write longer than the interval: 0, never negative
        AudioPerfSnapshot s{};
        c.snapshot(10000, s);
        check(s.task_busy_max_us == 3000 && s.task_busy_avg_us == (3000 + 1000 + 0) / 3,
              "B1 task busy per block = delivery interval - the write's wait (max " + std::to_string(s.task_busy_max_us) +
                  ", avg " + std::to_string(s.task_busy_avg_us) + " us)");

        // Against the ESP-IDF ring model: the task spends `cost` between a
        // write returning and the next write; the counter must say exactly that.
        VirtualI2sRing ring(kAudioRingBlocks);
        AudioRingPump pump;
        pump.set_music_library(&library);
        pump.set_clock({&ring, &ring_clock});
        pump.play_music(MusicSong::Theme);
        pump.reset_perf();
        for (int k = 0; k < 600; ++k) {
            ring.advance(k % 2 ? 2500 : 1500);
            pump.step(ring, 0, gain);
        }
        AudioPerfSnapshot m{};
        pump.perf(m);
        check(m.task_busy_max_us == 2500 && m.task_busy_avg_us >= 1990 && m.task_busy_avg_us <= 2010 &&
                  m.underruns == 0 && ring.dry_plays == 0,
              "B2 through the production pump and the descriptor-ring model: task busy max " +
                  std::to_string(m.task_busy_max_us) + " us, avg " + std::to_string(m.task_busy_avg_us) +
                  " us = the work the model spent per block (1.5 / 2.5 ms alternating)");
    }

    // ======================================================================
    // C -- the contention counters (section 20.2)
    // ======================================================================
    {
        ContentionCounters c;
        c.reset(1000000);
        c.on_frame(400, 60000, frame_timing(10000, 30000, true));  // viewport frame
        c.on_frame(800, 20000, frame_timing(4000, 10000, false));  // panel frame
        c.on_frame(600, 70000, frame_timing(12000, 40000, true));  // viewport frame
        c.on_loop(5000);
        c.on_loop(9000);
        ContentionSnapshot s{};
        c.snapshot(3000000, s);
        check(s.window_us == 2000000 && s.frames == 3 && s.timed && s.logic_avg_us == 600 && s.logic_max_us == 800,
              "C1 frames and game logic: 3 frames, logic avg 0.6 max 0.8 ms");
        check(s.viewport_frames == 2 && s.viewport_tft_avg_us == 65000 && s.viewport_tft_max_us == 70000 &&
                  s.panel_frames == 1 && s.panel_tft_avg_us == 20000 && s.panel_tft_max_us == 20000,
              "C2 whole-viewport frames and the rest are kept apart (65.0 avg / 70.0 max vs 20.0)");
        // fill = tft - xfer - yield: (20 + 6 + 18) / 3 ms
        check(s.tft_xfer_avg_us == (10000 + 4000 + 12000) / 3 && s.tft_xfer_max_us == 12000 &&
                  s.tft_yield_avg_us == (30000 + 10000 + 40000) / 3 && s.tft_yield_max_us == 40000 &&
                  s.tft_fill_avg_us == (20000 + 6000 + 18000) / 3,
              "C3 each frame's TFT time split into row building / SPI transfer / tick yields");
        check(s.transactions == 600 && s.rows == 540 && s.yields == 30 && s.late_yields == 3 &&
                  s.yield_max_us == 12000 && s.xfer_max_us == 1200 && s.slow_xfers == 3 && s.slow_xfers_busy == 3,
              "C4 transactions, rows, yields (late ones), the slowest transaction and the slow ones with audio running");
        check(s.rows_idle == 300 && s.rows_busy == 150 && s.row_fill_idle_x10 == 123 && s.row_fill_busy_x10 == 150 &&
                  s.row_xfer_idle_x10 == 650 && s.row_xfer_busy_x10 == 660,
              "C5 the same row code split by what core 1 was doing: fill 12.3 vs 15.0 us, xfer 65.0 vs 66.0 us a row");
        check(s.loops == 2 && s.loop_avg_us == 7000 && s.loop_max_us == 9000, "C6 main.cpp's loop passes");

        // A frame the Board did not time (the host, or no display) still counts as a frame.
        ContentionCounters u;
        u.reset(0);
        u.on_frame(100, 5000, TftTiming{});
        ContentionSnapshot us{};
        u.snapshot(1000, us);
        check(us.frames == 1 && !us.timed && us.tft_xfer_avg_us == 0 && us.rows == 0 && us.panel_frames == 1,
              "C7 an untimed frame counts as a frame but adds nothing to the TFT split");
        c.reset(5000000);
        ContentionSnapshot z{};
        c.snapshot(5000000, z);
        check(z.frames == 0 && !z.timed && z.loops == 0 && z.rows == 0 && z.window_us == 0,
              "C8 reset starts an empty window");
    }

    // ======================================================================
    // F -- the Developer report's contention section (section 20.5)
    // ======================================================================
    AudioPerfSnapshot audio{};
    audio.music_active = true;
    audio.song = MusicSong::Theme;
    audio.window_us = 20000000;
    audio.blocks = 2500;
    audio.render_avg_us = 2900;
    audio.render_max_us = 4100;
    audio.music_avg_us = 2700;
    audio.music_max_us = 3900;
    audio.task_busy_avg_us = 3000;
    audio.task_busy_max_us = 4300;
    audio.write_avg_us = 5000;
    audio.write_max_us = 7900;
    audio.fill_min = 7;
    audio.fill_max = 8;
    RenderPerfSnapshot render{};
    render.window_us = 20000000;
    render.frames = 250;
    render.frame_avg_us = 42100;
    render.frame_max_us = 120400;
    render.late_frames = 3;
    SystemPerfSnapshot system{};
    system.valid = true;
    system.core_busy_permille[0] = 980;
    system.core_busy_permille[1] = 360;
    system.main_permille = 950;
    system.audio_permille = 330;
    ContentionCounters cc;
    cc.reset(0);
    for (int i = 0; i < 9; ++i) cc.on_frame(400, 60000, frame_timing(10000, 30000, i % 3 != 0));
    cc.on_loop(130100);
    ContentionSnapshot contention{};
    cc.snapshot(20000000, contention);
    SdLogPerf sd{};
    sd.valid = true;
    sd.bursts = 3;
    sd.max_us = 12300;
    sd.busy_us = 20000;
    PerfScenario scenario{};
    scenario.music_available = true;
    scenario.music_volume = 80;
    scenario.sfx_volume = 70;
    {
        PerfReportInput in{};
        in.title = "AUDIO/RENDER PERF  live window";
        in.audio = &audio;
        in.audio_heading = "Since last read";
        in.render = &render;
        in.system = &system;
        in.scenario = &scenario;
        in.contention = &contention;
        in.sdlog = &sd;
        char lines[kPerfReportMaxLines][kPerfReportLineBytes]{};
        const size_t n = format_perf_report(in, lines, kPerfReportMaxLines);
        const std::string text = join(lines, n);
        const char *want[] = {"-- Contention map (A3-04C) --",
                              "music 80% Ultima V Theme sfx 70%",
                              "logic avg 0.4 max 0.4  loop max 130.1 ms",
                              "viewport 6 frm tft avg 60.0 max 60.0",
                              "other 3 frm tft avg 60.0 max 60.0",
                              "tft/frame fill 20.0 xfer 10.0 yield 30.0",
                              "yield 90 max 12.0 ms late 9 (tick 10)",
                              "xfer max 1.20 ms slow 9 (9 w/audio)",
                              "rows 1620  audio running at 33%",
                              "row fill us: audio idle 12.3 busy 15.0",
                              "row xfer us: audio idle 65.0 busy 66.0",
                              "audio task/blk avg 3.00 max 4.30 ms",
                              "sfx+mix avg 0.20 write avg 5.0 max 7.9",
                              "sd log 3 bursts max 12.3 total 20.0 ms"};
        std::string missing;
        for (const char *w : want)
            if (text.find(std::string(w) + "\n") == std::string::npos) missing += std::string(" [") + w + "]";
        check(missing.empty(), "F1 the report's contention section carries every field, whole lines:" +
                                   (missing.empty() ? std::string(" all present") : missing));
        scenario.synth_bypass = true;
        const size_t nb = format_perf_report(in, lines, kPerfReportMaxLines);
        check(join(lines, nb).find("music 80% Ultima V Theme sfx 70% BYPASS\n") != std::string::npos,
              "F2 the probe is named on the report while it is on (BYPASS)");
        scenario.synth_bypass = false;
        PerfScenario stock{};
        stock.sfx_volume = 80;
        in.scenario = &stock;
        const size_t ns = format_perf_report(in, lines, kPerfReportMaxLines);
        check(join(lines, ns).find("music n/a  sfx 80%\n") != std::string::npos,
              "F3 no music capability (stock files / no audio pack) is said, not left blank");
        in.scenario = &scenario;

        // The longest report: the benchmark's two audio phases, the guard, all of it.
        in.audio2 = &audio;
        in.audio2_heading = "Music + cue/100 ms";
        in.has_guard = true;
        in.guard_ns = 900;
        in.guard_us_per_block = 12500;
        const size_t nl = format_perf_report(in, lines, kPerfReportMaxLines);
        check(nl < kPerfReportMaxLines && std::string(lines[nl - 1]).find("A3-04 guard") == 0,
              "F4 the longest report (benchmark + contention) fits: " + std::to_string(nl) + " of " +
                  std::to_string(kPerfReportMaxLines) + " lines, the last line is the guard's (nothing cut)");
        check(nl > 48, "F4 ... which A3-04B's 48-line buffer would have cut (" + std::to_string(nl) + " lines)");

        // Every counter at its maximum: every line still fits a Developer row.
        AudioPerfSnapshot amax{};
        std::memset(&amax, 0xff, sizeof amax);
        amax.music_active = true;
        amax.song = MusicSong::Theme;
        amax.music_bypass = true;
        ContentionSnapshot cmax{};
        std::memset(&cmax, 0xff, sizeof cmax);
        cmax.timed = true;
        RenderPerfSnapshot rmax{};
        std::memset(&rmax, 0xff, sizeof rmax);
        SdLogPerf smax{};
        std::memset(&smax, 0xff, sizeof smax);
        smax.valid = true;
        PerfScenario scmax{};
        scmax.music_available = true;
        scmax.music_volume = scmax.sfx_volume = 255;
        scmax.synth_bypass = true;
        PerfReportInput big{};
        big.audio = &amax;
        big.audio2 = &amax;
        big.render = &rmax;
        big.system = &system;
        big.scenario = &scmax;
        big.contention = &cmax;
        big.sdlog = &smax;
        big.has_guard = true;
        const size_t nm = format_perf_report(big, lines, kPerfReportMaxLines);
        bool fits = nm > 0;
        for (size_t i = 0; i < nm; ++i) fits = fits && std::strlen(lines[i]) <= kPerfReportLineBytes - 1;
        check(fits, "F5 every line fits a 51-character Developer row with every counter at its maximum");
        in.scenario = nullptr;
        in.contention = nullptr;
        in.sdlog = nullptr;
        in.audio2 = nullptr;
        in.has_guard = false;
        const size_t n0 = format_perf_report(in, lines, kPerfReportMaxLines);
        check(join(lines, n0).find("Contention") == std::string::npos,
              "F6 without the A3-04C inputs the report is A3-04B's (no contention section)");
    }

    // ======================================================================
    // L -- the one-line A3C_PERF heartbeat (section 20.5)
    // ======================================================================
    {
        PerfReportInput in{};
        in.audio = &audio;
        in.render = &render;
        in.system = &system;
        in.scenario = &scenario;
        in.contention = &contention;
        in.sdlog = &sd;
        char line[kContentionLineBytes];
        const size_t n = format_contention_line(in, line, sizeof line);
        const std::string s(line);
        const char *order[] = {"scen=[music 80% Ultima V Theme sfx 70%]", " win=20.0s", " | frame n=250 avg=42.1",
                               "max=120.4 late=3", " | cmp=", " tiles=", " tft=", " in=",
                               " | logic=0.4/0.4 vp=6:60.0/60.0 oth=3:60.0/60.0",
                               " | split fill=20.0 xfer=10.0 yld=30.0", " | yld n=90 max=12.0 late=9",
                               " | xfer max=1.20 slow=9/9", " | rows=1620 busy=33% fill=12.3/15.0 xfer=65.0/66.0",
                               " | loop=1:130.1/130.1",
                               " | audio blk=2500 rnd=2.90/4.10 mus=2.70/3.90 task=4.30",
                               " buf=56 und=0 hw=0 miss=0 clip=0", " | sd=3:12.3/20.0",
                               " | cpu0=98 cpu1=36 main=95 aud=33 inp=0 sdl=0"};
        size_t at = 0;
        std::string broken;
        for (const char *f : order) {
            const auto p = s.find(f, at);
            if (p == std::string::npos) {
                broken += std::string(" [") + f + "]";
                continue;
            }
            at = p + std::strlen(f);
        }
        check(broken.empty() && n == s.size() && n < 540,
              "L1 the A3C_PERF line carries every field, in the documented order:" +
                  (broken.empty() ? " " + std::to_string(n) + " chars" : broken));

        // The worst realistic line (4-digit ms, 6-digit counts) is whole within the
        // buffer and within one SD-log record (768 B) with the ESP log prefix.
        RenderPerfSnapshot r9 = render;
        r9.frames = 999999;
        r9.frame_avg_us = r9.frame_p95_us = r9.frame_max_us = r9.compose_avg_us = r9.compose_max_us = 9999900;
        r9.tiles_max_us = r9.tft_avg_us = r9.tft_max_us = r9.input_max_us = 9999900;
        r9.late_frames = r9.shown = 999999;
        ContentionSnapshot c9 = contention;
        c9.frames = c9.viewport_frames = c9.panel_frames = c9.yields = c9.late_yields = c9.slow_xfers = 999999;
        c9.slow_xfers_busy = c9.loops = 999999;
        c9.rows = c9.rows_busy = c9.rows_idle = 99999999;
        c9.logic_avg_us = c9.logic_max_us = c9.viewport_tft_avg_us = c9.viewport_tft_max_us = 9999900;
        c9.panel_tft_avg_us = c9.panel_tft_max_us = c9.tft_fill_avg_us = c9.tft_xfer_avg_us = 9999900;
        c9.tft_yield_avg_us = c9.yield_max_us = c9.xfer_max_us = c9.loop_avg_us = c9.loop_max_us = 9999900;
        c9.row_fill_idle_x10 = c9.row_fill_busy_x10 = c9.row_xfer_idle_x10 = c9.row_xfer_busy_x10 = 9999999;
        AudioPerfSnapshot a9 = audio;
        a9.blocks = a9.underruns = a9.hw_underruns = a9.missed_deadlines = a9.mix_clipped = 999999;
        a9.render_avg_us = a9.render_max_us = a9.music_avg_us = a9.music_max_us = a9.task_busy_max_us = 9999990;
        SdLogPerf s9 = sd;
        s9.bursts = 999999;
        s9.max_us = s9.busy_us = 99999900;
        SystemPerfSnapshot y9 = system;
        y9.core_busy_permille[0] = y9.core_busy_permille[1] = y9.main_permille = y9.audio_permille = 1000;
        y9.input_permille = y9.sdlog_permille = 1000;
        PerfScenario sc9 = scenario;
        sc9.music_volume = sc9.sfx_volume = 100;
        sc9.synth_bypass = true;
        PerfReportInput worst{};
        worst.audio = &a9;
        worst.render = &r9;
        worst.system = &y9;
        worst.scenario = &sc9;
        worst.contention = &c9;
        worst.sdlog = &s9;
        const size_t nw = format_contention_line(worst, line, sizeof line);
        const std::string w(line);
        check(nw < kContentionLineBytes - 1 && w.find(" sdl=100") != std::string::npos &&
                  nw + std::strlen("I (4294967295) AlphaRuntime: A3C_PERF \n") < 768,
              "L2 the worst realistic line is whole (" + std::to_string(nw) + " chars, ends with sdl=100) and fits "
              "one 768-byte SD-log record with its log prefix");

        // Bounded: a tiny buffer is filled, terminated and never overrun.
        char tiny[24 + 8];
        std::memset(tiny, 0x5a, sizeof tiny);
        const size_t nt = format_contention_line(in, tiny, 24);
        check(nt == 23 && tiny[23] == 0 && tiny[24] == 0x5a && tiny[31] == 0x5a,
              "L3 a short buffer is filled, NUL-terminated and never overrun");
        PerfReportInput none{};
        none.scenario = &scenario;
        none.render = &render;
        format_contention_line(none, line, sizeof line);
        check(std::string(line).find(" | audio none") != std::string::npos,
              "L4 no audio window (nothing has played) says so");
    }

    // ======================================================================
    // S -- the device wiring (source scans; the device code does not run here)
    // ======================================================================
    {
        const std::string board = strip_comments(slurp(device_dir + "/main/tdeck_board.cpp"));
        const std::string transmit = function_body(board, "esp_err_t Board::tft_transmit(");
        const std::string yield = function_body(board, "void Board::tft_yield(");
        check(!board.empty() && count_of(board, "spi_device_transmit(") == 1 &&
                  transmit.find("spi_device_transmit(") != std::string::npos,
              "S1 every TFT transaction goes through the timed Board::tft_transmit (the only spi_device_transmit)");
        check(count_of(board, "vTaskDelay(1)") == 1 && yield.find("vTaskDelay(1)") != std::string::npos &&
                  count_of(board, "tft_yield()") >= 5,
              "S2 every draw-loop yield is the timed Board::tft_yield (the only vTaskDelay(1)); " +
                  std::to_string(count_of(board, "tft_yield()") - 1) + " loops use it");
        check(std::regex_search(transmit, std::regex(R"(audio_running\(\)[\s\S]*spi_device_transmit[\s\S]*audio_running\(\))")) &&
                  transmit.find("rows_busy") != std::string::npos && transmit.find("rows_idle") != std::string::npos,
              "S3 a row is classified by the audio task's flag at the row's start and around its transaction");
        check(count_of(board, "row_mark()") >= 7 && board.find("++tft_timing_.viewport_full") != std::string::npos,
              "S4 every row loop marks its row's start; the whole-viewport write is marked");

        const std::string audio_cpp = strip_comments(slurp(device_dir + "/main/tdeck_audio.cpp"));
        const std::string write = function_body(audio_cpp, "bool TdeckAudioBackend::I2sRingSink::write(");
        const std::string run = function_body(audio_cpp, "void TdeckAudioBackend::run(");
        check(std::regex_search(write, std::regex(R"(active_ = 0;[\s\S]*i2s_channel_write[\s\S]*active_ = 1;)")) &&
                  std::regex_search(run, std::regex(R"(active_ = 0;[\s\S]*xQueueReceive[\s\S]*active_ = 1;)")) &&
                  std::regex_search(run, std::regex(R"(active_ = 0;\s*vTaskDelay\(1\);\s*active_ = 1;)")),
              "S5 the audio task's flag is 0 exactly around its three blocking waits (the write, the idle queue "
              "wait, the runaway yield)");
        check(std::regex_search(run, std::regex(R"(set_music_bypass\(bypass_requested_\.load\(\)\);[\s\S]*pump_\.step)")),
              "S6 the probe reaches the pump between blocks, before the step that renders");

        const std::string main_cpp = strip_comments(slurp(device_dir + "/main/main.cpp"));
        check(main_cpp.find("board.set_audio_activity_flag(audio_backend.activity_flag())") != std::string::npos &&
                  main_cpp.find("runtime.attach_sd_log_perf(") != std::string::npos &&
                  main_cpp.find("runtime.note_loop_pass(") != std::string::npos,
              "S7 main.cpp attaches the audio flag to the Board, the SD-log bursts and the loop passes");
        const std::string sdlog = strip_comments(slurp(device_dir + "/main/sd_diagnostic_logger.cpp"));
        check(std::regex_search(sdlog, std::regex(R"(xSemaphoreTake\(g_storage_mutex[\s\S]*burst_start_us[\s\S]*g_perf_bursts\.fetch_add[\s\S]*xSemaphoreGive)")),
              "S8 the SD-log burst is timed from taking the storage mutex to giving it back");
        const std::string runtime = strip_comments(slurp(device_dir + "/main/alpha_runtime.cpp"));
        check(runtime.find("\"A3C_PERF %s\"") != std::string::npos &&
                  std::regex_search(runtime, std::regex(R"(contention_\.on_frame\(uint32_t\(frame_t0-logic_t0\))")),
              "S9 the runtime counts every gameplay frame's logic and TFT split and logs the A3C_PERF line");
        const std::string lf = slurp(device_dir + "/main/audio_iram.lf");
        check(lf.find("_ZN6openu513AudioRingPump12render_blockEtt (noflash)") != std::string::npos,
              "S10 the probe's check lives in render_block, which stays in IRAM (audio_iram.lf unchanged)");
    }

    std::printf("\n%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
